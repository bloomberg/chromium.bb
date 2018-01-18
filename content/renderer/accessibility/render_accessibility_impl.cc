// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/accessibility/render_accessibility_impl.h"

#include <stddef.h>
#include <stdint.h>

#include "base/bind.h"
#include "base/containers/queue.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "content/common/accessibility_messages.h"
#include "content/renderer/accessibility/blink_ax_enum_conversion.h"
#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_view_impl.h"
#include "third_party/WebKit/public/platform/TaskType.h"
#include "third_party/WebKit/public/platform/WebFloatRect.h"
#include "third_party/WebKit/public/web/WebAXObject.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebInputElement.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebSettings.h"
#include "third_party/WebKit/public/web/WebUserGestureIndicator.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "ui/accessibility/ax_node.h"

using blink::WebAXObject;
using blink::WebDocument;
using blink::WebElement;
using blink::WebFloatRect;
using blink::WebLocalFrame;
using blink::WebNode;
using blink::WebPoint;
using blink::WebRect;
using blink::WebScopedAXContext;
using blink::WebSettings;
using blink::WebView;

namespace {
// The next token to use to distinguish between ack events sent to this
// RenderAccessibilityImpl and a previous instance.
static int g_next_ack_token = 1;
}

namespace content {

// Cap the number of nodes returned in an accessibility
// tree snapshot to avoid outrageous memory or bandwidth
// usage.
const size_t kMaxSnapshotNodeCount = 5000;

// static
void RenderAccessibilityImpl::SnapshotAccessibilityTree(
    RenderFrameImpl* render_frame,
    AXContentTreeUpdate* response) {
  TRACE_EVENT0("accessibility",
               "RenderAccessibilityImpl::SnapshotAccessibilityTree");

  DCHECK(render_frame);
  DCHECK(response);
  if (!render_frame->GetWebFrame())
    return;

  WebDocument document = render_frame->GetWebFrame()->GetDocument();
  WebScopedAXContext context(document);
  WebAXObject root = context.Root();
  if (!root.UpdateLayoutAndCheckValidity())
    return;
  BlinkAXTreeSource tree_source(
      render_frame, ui::AXMode::kNativeAPIs | ui::AXMode::kWebContents |
                        ui::AXMode::kInlineTextBoxes |
                        ui::AXMode::kScreenReader | ui::AXMode::kHTML);
  tree_source.SetRoot(root);
  ScopedFreezeBlinkAXTreeSource freeze(&tree_source);
  BlinkAXTreeSerializer serializer(&tree_source);
  serializer.set_max_node_count(kMaxSnapshotNodeCount);

  if (serializer.SerializeChanges(context.Root(), response))
    return;

  // It's possible for the page to fail to serialize the first time due to
  // aria-owns rearranging the page while it's being scanned. Try a second
  // time.
  *response = AXContentTreeUpdate();
  if (serializer.SerializeChanges(context.Root(), response))
    return;

  // It failed again. Clear the response object because it might have errors.
  *response = AXContentTreeUpdate();
  LOG(WARNING) << "Unable to serialize accessibility tree.";
}

RenderAccessibilityImpl::RenderAccessibilityImpl(RenderFrameImpl* render_frame,
                                                 ui::AXMode mode)
    : RenderFrameObserver(render_frame),
      render_frame_(render_frame),
      tree_source_(render_frame, mode),
      serializer_(&tree_source_),
      plugin_tree_source_(nullptr),
      last_scroll_offset_(gfx::Size()),
      ack_pending_(false),
      reset_token_(0),
      during_action_(false),
      weak_factory_(this) {
  ack_token_ = g_next_ack_token++;
  WebView* web_view = render_frame_->GetRenderView()->GetWebView();
  WebSettings* settings = web_view->GetSettings();
  settings->SetAccessibilityEnabled(true);

#if defined(OS_ANDROID)
  // Password values are only passed through on Android.
  settings->SetAccessibilityPasswordValuesEnabled(true);
#endif

#if !defined(OS_ANDROID)
  // Inline text boxes can be enabled globally on all except Android.
  // On Android they can be requested for just a specific node.
  if (mode.has_mode(ui::AXMode::kInlineTextBoxes))
    settings->SetInlineTextBoxAccessibilityEnabled(true);
#endif

  const WebDocument& document = GetMainDocument();
  if (!document.IsNull()) {
    // It's possible that the webview has already loaded a webpage without
    // accessibility being enabled. Initialize the browser's cached
    // accessibility tree by sending it a notification.
    HandleAXEvent(WebAXObject::FromWebDocument(document),
                  ui::AX_EVENT_LAYOUT_COMPLETE);
  }
}

RenderAccessibilityImpl::~RenderAccessibilityImpl() {
}

void RenderAccessibilityImpl::AccessibilityModeChanged() {
  ui::AXMode new_mode = render_frame_->accessibility_mode();
  if (tree_source_.accessibility_mode() == new_mode)
    return;
  tree_source_.SetAccessibilityMode(new_mode);

#if !defined(OS_ANDROID)
  // Inline text boxes can be enabled globally on all except Android.
  // On Android they can be requested for just a specific node.
  RenderView* render_view = render_frame_->GetRenderView();
  if (render_view) {
    WebView* web_view = render_view->GetWebView();
    if (web_view) {
      WebSettings* settings = web_view->GetSettings();
      if (settings) {
        if (new_mode.has_mode(ui::AXMode::kInlineTextBoxes)) {
          settings->SetInlineTextBoxAccessibilityEnabled(true);
          tree_source_.GetRoot().LoadInlineTextBoxes();
        } else {
          settings->SetInlineTextBoxAccessibilityEnabled(false);
        }
      }
    }
  }
#endif  // !defined(OS_ANDROID)

  serializer_.Reset();
  const WebDocument& document = GetMainDocument();
  if (!document.IsNull()) {
    // If there are any events in flight, |HandleAXEvent| will refuse to process
    // our new event.
    pending_events_.clear();
    auto webax_object = WebAXObject::FromWebDocument(document);
    ui::AXEvent event = webax_object.IsLoaded() ? ui::AX_EVENT_LOAD_COMPLETE
                                                : ui::AX_EVENT_LAYOUT_COMPLETE;
    HandleAXEvent(webax_object, event);
  }
}

bool RenderAccessibilityImpl::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  during_action_ = true;
  IPC_BEGIN_MESSAGE_MAP(RenderAccessibilityImpl, message)

    IPC_MESSAGE_HANDLER(AccessibilityMsg_PerformAction, OnPerformAction)
    IPC_MESSAGE_HANDLER(AccessibilityMsg_Events_ACK, OnEventsAck)
    IPC_MESSAGE_HANDLER(AccessibilityMsg_HitTest, OnHitTest)
    IPC_MESSAGE_HANDLER(AccessibilityMsg_Reset, OnReset)
    IPC_MESSAGE_HANDLER(AccessibilityMsg_FatalError, OnFatalError)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  during_action_ = false;
  return handled;
}

void RenderAccessibilityImpl::HandleWebAccessibilityEvent(
    const blink::WebAXObject& obj, blink::WebAXEvent event) {
  HandleAXEvent(obj, AXEventFromBlink(event));
}

void RenderAccessibilityImpl::HandleAccessibilityFindInPageResult(
    int identifier,
    int match_index,
    const blink::WebAXObject& start_object,
    int start_offset,
    const blink::WebAXObject& end_object,
    int end_offset) {
  AccessibilityHostMsg_FindInPageResultParams params;
  params.request_id = identifier;
  params.match_index = match_index;
  params.start_id = start_object.AxID();
  params.start_offset = start_offset;
  params.end_id = end_object.AxID();
  params.end_offset = end_offset;
  Send(new AccessibilityHostMsg_FindInPageResult(routing_id(), params));
}

void RenderAccessibilityImpl::AccessibilityFocusedNodeChanged(
    const WebNode& node) {
  const WebDocument& document = GetMainDocument();
  if (document.IsNull())
    return;

  if (node.IsNull()) {
    // When focus is cleared, implicitly focus the document.
    // TODO(dmazzoni): Make Blink send this notification instead.
    HandleAXEvent(WebAXObject::FromWebDocument(document), ui::AX_EVENT_BLUR);
  }
}

void RenderAccessibilityImpl::DisableAccessibility() {
  RenderView* render_view = render_frame_->GetRenderView();
  if (!render_view)
    return;

  WebView* web_view = render_view->GetWebView();
  if (!web_view)
    return;

  WebSettings* settings = web_view->GetSettings();
  if (!settings)
    return;

  settings->SetAccessibilityEnabled(false);
}

void RenderAccessibilityImpl::HandleAXEvent(const blink::WebAXObject& obj,
                                            ui::AXEvent event,
                                            int action_request_id) {
  const WebDocument& document = GetMainDocument();
  if (document.IsNull())
    return;

  if (document.GetFrame()) {
    gfx::Size scroll_offset = document.GetFrame()->GetScrollOffset();
    if (scroll_offset != last_scroll_offset_) {
      // Make sure the browser is always aware of the scroll position of
      // the root document element by posting a generic notification that
      // will update it.
      // TODO(dmazzoni): remove this as soon as
      // https://bugs.webkit.org/show_bug.cgi?id=73460 is fixed.
      last_scroll_offset_ = scroll_offset;
      auto webax_object = WebAXObject::FromWebDocument(document);
      if (!obj.Equals(webax_object)) {
        HandleAXEvent(webax_object, ui::AX_EVENT_LAYOUT_COMPLETE);
      }
    }
  }

#if defined(OS_ANDROID)
  // Force the newly focused node to be re-serialized so we include its
  // inline text boxes.
  if (event == ui::AX_EVENT_FOCUS)
    serializer_.DeleteClientSubtree(obj);
#endif

  // If some cell IDs have been added or removed, we need to update the whole
  // table.
  if (obj.Role() == blink::kWebAXRoleRow &&
      event == ui::AX_EVENT_CHILDREN_CHANGED) {
    WebAXObject table_like_object = obj.ParentObject();
    if (!table_like_object.IsDetached()) {
      serializer_.DeleteClientSubtree(table_like_object);
      HandleAXEvent(table_like_object, ui::AX_EVENT_CHILDREN_CHANGED);
    }
  }

  // If a select tag is opened or closed, all the children must be updated
  // because their visibility may have changed.
  if (obj.Role() == blink::kWebAXRoleMenuListPopup &&
      event == ui::AX_EVENT_CHILDREN_CHANGED) {
    WebAXObject popup_like_object = obj.ParentObject();
    if (!popup_like_object.IsDetached()) {
      serializer_.DeleteClientSubtree(popup_like_object);
      HandleAXEvent(popup_like_object, ui::AX_EVENT_CHILDREN_CHANGED);
    }
  }

  // Add the accessibility object to our cache and ensure it's valid.
  AccessibilityHostMsg_EventParams acc_event;
  acc_event.id = obj.AxID();
  acc_event.event_type = event;

  if (blink::WebUserGestureIndicator::IsProcessingUserGesture(
          render_frame_->GetWebFrame())) {
    acc_event.event_from = ui::AX_EVENT_FROM_USER;
  } else if (during_action_) {
    acc_event.event_from = ui::AX_EVENT_FROM_ACTION;
  } else {
    acc_event.event_from = ui::AX_EVENT_FROM_PAGE;
  }

  acc_event.action_request_id = action_request_id;

  // Discard duplicate accessibility events.
  for (uint32_t i = 0; i < pending_events_.size(); ++i) {
    if (pending_events_[i].id == acc_event.id &&
        pending_events_[i].event_type == acc_event.event_type) {
      return;
    }
  }
  pending_events_.push_back(acc_event);

  // Don't send accessibility events for frames that are not in the frame tree
  // yet (i.e., provisional frames used for remote-to-local navigations, which
  // haven't committed yet).  Doing so might trigger layout, which may not work
  // correctly for those frames.  The events should be sent once such a frame
  // commits.
  if (!render_frame_->in_frame_tree())
    return;

  if (!ack_pending_ && !weak_factory_.HasWeakPtrs()) {
    // When no accessibility events are in-flight post a task to send
    // the events to the browser. We use PostTask so that we can queue
    // up additional events.
    render_frame_->GetTaskRunner(blink::TaskType::kUnspecedTimer)
        ->PostTask(FROM_HERE,
                   base::BindOnce(
                       &RenderAccessibilityImpl::SendPendingAccessibilityEvents,
                       weak_factory_.GetWeakPtr()));
  }
}

int RenderAccessibilityImpl::GenerateAXID() {
  WebAXObject root = tree_source_.GetRoot();
  return root.GenerateAXID();
}

void RenderAccessibilityImpl::SetPluginTreeSource(
    RenderAccessibilityImpl::PluginAXTreeSource* plugin_tree_source) {
  plugin_tree_source_ = plugin_tree_source;
  plugin_serializer_.reset(new PluginAXTreeSerializer(plugin_tree_source_));

  OnPluginRootNodeUpdated();
}

void RenderAccessibilityImpl::OnPluginRootNodeUpdated() {
  // Search the accessibility tree for an EMBED element and post a
  // children changed notification on it to force it to update the
  // plugin accessibility tree.

  ScopedFreezeBlinkAXTreeSource freeze(&tree_source_);
  WebAXObject root = tree_source_.GetRoot();
  if (!root.UpdateLayoutAndCheckValidity())
    return;

  base::queue<WebAXObject> objs_to_explore;
  objs_to_explore.push(root);
  while (objs_to_explore.size()) {
    WebAXObject obj = objs_to_explore.front();
    objs_to_explore.pop();

    WebNode node = obj.GetNode();
    if (!node.IsNull() && node.IsElementNode()) {
      WebElement element = node.To<WebElement>();
      if (element.HasHTMLTagName("embed")) {
        HandleAXEvent(obj, ui::AX_EVENT_CHILDREN_CHANGED);
        break;
      }
    }

    // Explore children of this object.
    std::vector<blink::WebAXObject> children;
    tree_source_.GetChildren(obj, &children);
    for (size_t i = 0; i < children.size(); ++i)
      objs_to_explore.push(children[i]);
  }
}

WebDocument RenderAccessibilityImpl::GetMainDocument() {
  if (render_frame_->GetWebFrame())
    return render_frame_->GetWebFrame()->GetDocument();
  return WebDocument();
}

void RenderAccessibilityImpl::SendPendingAccessibilityEvents() {
  TRACE_EVENT0("accessibility",
               "RenderAccessibilityImpl::SendPendingAccessibilityEvents");

  const WebDocument& document = GetMainDocument();
  if (document.IsNull())
    return;

  if (pending_events_.empty())
    return;

  ack_pending_ = true;

  // Make a copy of the events, because it's possible that
  // actions inside this loop will cause more events to be
  // queued up.
  std::vector<AccessibilityHostMsg_EventParams> src_events = pending_events_;
  pending_events_.clear();

  // Generate an event message from each Blink event.
  std::vector<AccessibilityHostMsg_EventParams> event_msgs;

  // If there's a layout complete message, we need to send location changes.
  bool had_layout_complete_messages = false;

  // Loop over each event and generate an updated event message.
  for (size_t i = 0; i < src_events.size(); ++i) {
    AccessibilityHostMsg_EventParams& event = src_events[i];
    if (event.event_type == ui::AX_EVENT_LAYOUT_COMPLETE)
      had_layout_complete_messages = true;

    auto obj = WebAXObject::FromWebDocumentByID(document, event.id);

    // Make sure the object still exists.
    if (!obj.UpdateLayoutAndCheckValidity())
      continue;

    // If it's ignored, find the first ancestor that's not ignored.
    while (!obj.IsDetached() && obj.AccessibilityIsIgnored())
      obj = obj.ParentObject();

    ScopedFreezeBlinkAXTreeSource freeze(&tree_source_);

    // Make sure it's a descendant of our root node - exceptions include the
    // scroll area that's the parent of the main document (we ignore it), and
    // possibly nodes attached to a different document.
    if (!tree_source_.IsInTree(obj))
      continue;

    AccessibilityHostMsg_EventParams event_msg;
    event_msg.event_type = event.event_type;
    event_msg.id = event.id;
    event_msg.event_from = event.event_from;
    event_msg.action_request_id = event.action_request_id;
    if (!serializer_.SerializeChanges(obj, &event_msg.update)) {
      VLOG(1) << "Failed to serialize one accessibility event.";
      continue;
    }

    if (plugin_tree_source_)
      AddPluginTreeToUpdate(&event_msg.update);

    event_msgs.push_back(event_msg);

    // For each node in the update, set the location in our map from
    // ids to locations.
    for (size_t j = 0; j < event_msg.update.nodes.size(); ++j) {
      ui::AXNodeData& src = event_msg.update.nodes[j];
      ui::AXRelativeBounds& dst = locations_[event_msg.update.nodes[j].id];
      dst.offset_container_id = src.offset_container_id;
      dst.bounds = src.location;
      dst.transform.reset(nullptr);
      if (src.transform)
        dst.transform.reset(new gfx::Transform(*src.transform));
    }

    VLOG(1) << "Accessibility event: " << ui::ToString(event.event_type)
            << " on node id " << event_msg.id
            << "\n" << event_msg.update.ToString();
  }

  Send(new AccessibilityHostMsg_Events(routing_id(), event_msgs, reset_token_,
                                       ack_token_));
  reset_token_ = 0;

  if (had_layout_complete_messages)
    SendLocationChanges();
}

void RenderAccessibilityImpl::SendLocationChanges() {
  TRACE_EVENT0("accessibility", "RenderAccessibilityImpl::SendLocationChanges");

  std::vector<AccessibilityHostMsg_LocationChangeParams> messages;

  // Update layout on the root of the tree.
  ScopedFreezeBlinkAXTreeSource freeze(&tree_source_);
  WebAXObject root = tree_source_.GetRoot();
  if (!root.UpdateLayoutAndCheckValidity())
    return;

  // Do a breadth-first explore of the whole blink AX tree.
  base::hash_map<int, ui::AXRelativeBounds> new_locations;
  base::queue<WebAXObject> objs_to_explore;
  objs_to_explore.push(root);
  while (objs_to_explore.size()) {
    WebAXObject obj = objs_to_explore.front();
    objs_to_explore.pop();

    // See if we had a previous location. If not, this whole subtree must
    // be new, so don't continue to explore this branch.
    int id = obj.AxID();
    auto iter = locations_.find(id);
    if (iter == locations_.end())
      continue;

    // If the location has changed, append it to the IPC message.
    WebAXObject offset_container;
    WebFloatRect bounds_in_container;
    SkMatrix44 container_transform;
    obj.GetRelativeBounds(offset_container, bounds_in_container,
                          container_transform);
    ui::AXRelativeBounds new_location;
    new_location.offset_container_id = offset_container.AxID();
    new_location.bounds = bounds_in_container;
    if (!container_transform.isIdentity())
      new_location.transform = base::WrapUnique(
          new gfx::Transform(container_transform));
    if (iter->second != new_location) {
      AccessibilityHostMsg_LocationChangeParams message;
      message.id = id;
      message.new_location = new_location;
      messages.push_back(message);
    }

    // Save the new location.
    new_locations[id] = new_location;

    // Explore children of this object.
    std::vector<blink::WebAXObject> children;
    tree_source_.GetChildren(obj, &children);
    for (size_t i = 0; i < children.size(); ++i)
      objs_to_explore.push(children[i]);
  }
  locations_.swap(new_locations);

  Send(new AccessibilityHostMsg_LocationChanges(routing_id(), messages));
}

void RenderAccessibilityImpl::OnPerformAction(
    const ui::AXActionData& data) {
  const WebDocument& document = GetMainDocument();
  if (document.IsNull())
    return;

  auto root = WebAXObject::FromWebDocument(document);
  if (!root.UpdateLayoutAndCheckValidity())
    return;

  auto target = WebAXObject::FromWebDocumentByID(document, data.target_node_id);
  auto anchor = WebAXObject::FromWebDocumentByID(document, data.anchor_node_id);
  auto focus = WebAXObject::FromWebDocumentByID(document, data.focus_node_id);

  switch (data.action) {
    case ui::AX_ACTION_BLUR:
      root.Focus();
      break;
    case ui::AX_ACTION_DECREMENT:
      target.Decrement();
      break;
    case ui::AX_ACTION_DO_DEFAULT:
      target.Click();
      break;
    case ui::AX_ACTION_GET_IMAGE_DATA:
      OnGetImageData(target, data.target_rect.size());
      break;
    case ui::AX_ACTION_HIT_TEST:
      DCHECK(data.hit_test_event_to_fire != ui::AX_EVENT_NONE);
      OnHitTest(data.target_point, data.hit_test_event_to_fire,
                data.request_id);
      break;
    case ui::AX_ACTION_INCREMENT:
      target.Increment();
      break;
    case ui::AX_ACTION_SCROLL_TO_MAKE_VISIBLE:
      target.ScrollToMakeVisibleWithSubFocus(
          WebRect(data.target_rect.x(), data.target_rect.y(),
                  data.target_rect.width(), data.target_rect.height()));
      break;
    case ui::AX_ACTION_SCROLL_TO_POINT:
      target.ScrollToGlobalPoint(
          WebPoint(data.target_point.x(), data.target_point.y()));
      break;
    case ui::AX_ACTION_LOAD_INLINE_TEXT_BOXES:
      OnLoadInlineTextBoxes(target);
      break;
    case ui::AX_ACTION_FOCUS:
      target.Focus();
      break;
    case ui::AX_ACTION_SET_SCROLL_OFFSET:
      target.SetScrollOffset(
          WebPoint(data.target_point.x(), data.target_point.y()));
      break;
    case ui::AX_ACTION_SET_SELECTION:
      anchor.SetSelection(anchor, data.anchor_offset, focus, data.focus_offset);
      HandleAXEvent(root, ui::AX_EVENT_LAYOUT_COMPLETE);
      break;
    case ui::AX_ACTION_SET_SEQUENTIAL_FOCUS_NAVIGATION_STARTING_POINT:
      target.SetSequentialFocusNavigationStartingPoint();
      break;
    case ui::AX_ACTION_SET_VALUE:
      target.SetValue(blink::WebString::FromUTF16(data.value));
      HandleAXEvent(target, ui::AX_EVENT_VALUE_CHANGED);
      break;
    case ui::AX_ACTION_SHOW_CONTEXT_MENU:
      target.ShowContextMenu();
      break;
    case ui::AX_ACTION_CUSTOM_ACTION:
    case ui::AX_ACTION_REPLACE_SELECTED_TEXT:
    case ui::AX_ACTION_SCROLL_BACKWARD:
    case ui::AX_ACTION_SCROLL_FORWARD:
    case ui::AX_ACTION_SCROLL_UP:
    case ui::AX_ACTION_SCROLL_DOWN:
    case ui::AX_ACTION_SCROLL_LEFT:
    case ui::AX_ACTION_SCROLL_RIGHT:
    case ui::AX_ACTION_NONE:
      NOTREACHED();
      break;
  }
}

void RenderAccessibilityImpl::OnEventsAck(int ack_token) {
  // Ignore acks intended for a different or previous instance.
  if (ack_token_ != ack_token)
    return;

  DCHECK(ack_pending_);
  ack_pending_ = false;
  SendPendingAccessibilityEvents();
}

void RenderAccessibilityImpl::OnFatalError() {
  CHECK(false) << "Invalid accessibility tree.";
}

void RenderAccessibilityImpl::OnHitTest(const gfx::Point& point,
                                        ui::AXEvent event_to_fire,
                                        int action_request_id) {
  const WebDocument& document = GetMainDocument();
  if (document.IsNull())
    return;
  auto root_obj = WebAXObject::FromWebDocument(document);
  if (!root_obj.UpdateLayoutAndCheckValidity())
    return;

  WebAXObject obj = root_obj.HitTest(point);
  if (obj.IsDetached())
    return;

  // If the object that was hit has a child frame, we have to send a
  // message back to the browser to do the hit test in the child frame,
  // recursively.
  AXContentNodeData data;
  ScopedFreezeBlinkAXTreeSource freeze(&tree_source_);
  tree_source_.SerializeNode(obj, &data);
  if (data.HasContentIntAttribute(AX_CONTENT_ATTR_CHILD_ROUTING_ID) ||
      data.HasContentIntAttribute(
          AX_CONTENT_ATTR_CHILD_BROWSER_PLUGIN_INSTANCE_ID)) {
    Send(new AccessibilityHostMsg_ChildFrameHitTestResult(
        routing_id(), action_request_id, point,
        data.GetContentIntAttribute(AX_CONTENT_ATTR_CHILD_ROUTING_ID),
        data.GetContentIntAttribute(
            AX_CONTENT_ATTR_CHILD_BROWSER_PLUGIN_INSTANCE_ID),
        event_to_fire));
    return;
  }

  // Otherwise, send an event on the node that was hit.
  HandleAXEvent(obj, event_to_fire, action_request_id);
}

void RenderAccessibilityImpl::OnLoadInlineTextBoxes(
    const blink::WebAXObject& obj) {
  ScopedFreezeBlinkAXTreeSource freeze(&tree_source_);
  if (tree_source_.ShouldLoadInlineTextBoxes(obj))
    return;

  tree_source_.SetLoadInlineTextBoxesForId(obj.AxID());

  const WebDocument& document = GetMainDocument();
  if (document.IsNull())
    return;

  // This object may not be a leaf node. Force the whole subtree to be
  // re-serialized.
  serializer_.DeleteClientSubtree(obj);

  // Explicitly send a tree change update event now.
  HandleAXEvent(obj, ui::AX_EVENT_TREE_CHANGED);
}

void RenderAccessibilityImpl::OnGetImageData(
    const blink::WebAXObject& obj, const gfx::Size& max_size) {
  ScopedFreezeBlinkAXTreeSource freeze(&tree_source_);
  if (tree_source_.image_data_node_id() == obj.AxID())
    return;

  tree_source_.set_image_data_node_id(obj.AxID());
  tree_source_.set_max_image_data_size(max_size);

  const WebDocument& document = GetMainDocument();
  if (document.IsNull())
    return;

  serializer_.DeleteClientSubtree(obj);
  HandleAXEvent(obj, ui::AX_EVENT_IMAGE_FRAME_UPDATED);
}

void RenderAccessibilityImpl::OnReset(int reset_token) {
  reset_token_ = reset_token;
  serializer_.Reset();
  pending_events_.clear();

  const WebDocument& document = GetMainDocument();
  if (!document.IsNull()) {
    // Tree-only mode gets used by the automation extension API which requires a
    // load complete event to invoke listener callbacks.
    auto webax_object = WebAXObject::FromWebDocument(document);
    ui::AXEvent evt = webax_object.IsLoaded() ? ui::AX_EVENT_LOAD_COMPLETE
                                              : ui::AX_EVENT_LAYOUT_COMPLETE;
    HandleAXEvent(webax_object, evt);
  }
}

void RenderAccessibilityImpl::OnDestruct() {
  delete this;
}

void RenderAccessibilityImpl::AddPluginTreeToUpdate(
    AXContentTreeUpdate* update) {
  for (size_t i = 0; i < update->nodes.size(); ++i) {
    if (update->nodes[i].role == ui::AX_ROLE_EMBEDDED_OBJECT) {
      const ui::AXNode* root = plugin_tree_source_->GetRoot();
      update->nodes[i].child_ids.push_back(root->id());

      ui::AXTreeUpdate plugin_update;
      plugin_serializer_->SerializeChanges(root, &plugin_update);

      // We have to copy the updated nodes using a loop because we're
      // converting from a generic ui::AXNodeData to a vector of its
      // content-specific subclass AXContentNodeData.
      size_t old_count = update->nodes.size();
      size_t new_count = plugin_update.nodes.size();
      update->nodes.resize(old_count + new_count);
      for (size_t j = 0; j < new_count; ++j)
        update->nodes[old_count + j] = plugin_update.nodes[j];
      break;
    }
  }
}

void RenderAccessibilityImpl::ScrollPlugin(int id_to_make_visible) {
  // Plugin content doesn't scroll itself, so when we're requested to
  // scroll to make a particular plugin node visible, get the
  // coordinates of the target plugin node and then tell the document
  // node to scroll to those coordinates.
  //
  // Note that calling scrollToMakeVisibleWithSubFocus() is preferable to
  // telling the document to scroll to a specific coordinate because it will
  // first compute whether that rectangle is visible and do nothing if it is.
  // If it's not visible, it will automatically center it.

  DCHECK(plugin_tree_source_);
  ui::AXNodeData root_data = plugin_tree_source_->GetRoot()->data();
  ui::AXNodeData target_data =
      plugin_tree_source_->GetFromId(id_to_make_visible)->data();

  gfx::RectF bounds = target_data.location;
  if (root_data.transform)
    root_data.transform->TransformRect(&bounds);

  const WebDocument& document = GetMainDocument();
  if (document.IsNull())
    return;

  WebAXObject::FromWebDocument(document).ScrollToMakeVisibleWithSubFocus(
      WebRect(bounds.x(), bounds.y(), bounds.width(), bounds.height()));
}

}  // namespace content
