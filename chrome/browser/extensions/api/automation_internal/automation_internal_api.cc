// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/automation_internal/automation_internal_api.h"

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/api/automation_internal/automation_event_router.h"
#include "chrome/browser/extensions/api/tabs/tabs_constants.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/extensions/api/automation_api_constants.h"
#include "chrome/common/extensions/api/automation_internal.h"
#include "chrome/common/extensions/chrome_extension_messages.h"
#include "chrome/common/extensions/manifest_handlers/automation.h"
#include "content/public/browser/ax_event_notification_details.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_plugin_guest_manager.h"
#include "content/public/browser/media_session.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/permissions/permissions_data.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_host_delegate.h"
#include "ui/accessibility/ax_tree_id_registry.h"

#if defined(USE_AURA)
#include "chrome/browser/ui/aura/accessibility/automation_manager_aura.h"
#include "ui/aura/env.h"
#endif

namespace extensions {
class AutomationWebContentsObserver;
}  // namespace extensions

DEFINE_WEB_CONTENTS_USER_DATA_KEY(extensions::AutomationWebContentsObserver);

namespace extensions {

namespace {

const char kCannotRequestAutomationOnPage[] =
    "Cannot request automation tree on url \"*\". "
    "Extension manifest must request permission to access this host.";
const char kRendererDestroyed[] = "The tab was closed.";
const char kNoDocument[] = "No document.";
const char kNodeDestroyed[] =
    "domQuerySelector sent on node which is no longer in the tree.";

// Handles sending and receiving IPCs for a single querySelector request. On
// creation, sends the request IPC, and is destroyed either when the response is
// received or the renderer is destroyed.
class QuerySelectorHandler : public content::WebContentsObserver {
 public:
  QuerySelectorHandler(
      content::WebContents* web_contents,
      int request_id,
      int acc_obj_id,
      const base::string16& query,
      const extensions::AutomationInternalQuerySelectorFunction::Callback&
          callback)
      : content::WebContentsObserver(web_contents),
        request_id_(request_id),
        callback_(callback) {
    content::RenderFrameHost* rfh = web_contents->GetMainFrame();

    rfh->Send(new ExtensionMsg_AutomationQuerySelector(
        rfh->GetRoutingID(), request_id, acc_obj_id, query));
  }

  ~QuerySelectorHandler() override {}

  bool OnMessageReceived(const IPC::Message& message,
                         content::RenderFrameHost* render_frame_host) override {
    if (message.type() != ExtensionHostMsg_AutomationQuerySelector_Result::ID)
      return false;

    // There may be several requests in flight; check this response matches.
    int message_request_id = 0;
    base::PickleIterator iter(message);
    if (!iter.ReadInt(&message_request_id))
      return false;

    if (message_request_id != request_id_)
      return false;

    IPC_BEGIN_MESSAGE_MAP(QuerySelectorHandler, message)
      IPC_MESSAGE_HANDLER(ExtensionHostMsg_AutomationQuerySelector_Result,
                          OnQueryResponse)
    IPC_END_MESSAGE_MAP()
    return true;
  }

  void WebContentsDestroyed() override {
    callback_.Run(kRendererDestroyed, 0);
    delete this;
  }

 private:
  void OnQueryResponse(int request_id,
                       ExtensionHostMsg_AutomationQuerySelector_Error error,
                       int result_acc_obj_id) {
    std::string error_string;
    switch (error.value) {
      case ExtensionHostMsg_AutomationQuerySelector_Error::kNone:
        break;
      case ExtensionHostMsg_AutomationQuerySelector_Error::kNoDocument:
        error_string = kNoDocument;
        break;
      case ExtensionHostMsg_AutomationQuerySelector_Error::kNodeDestroyed:
        error_string = kNodeDestroyed;
        break;
    }
    callback_.Run(error_string, result_acc_obj_id);
    delete this;
  }

  int request_id_;
  const extensions::AutomationInternalQuerySelectorFunction::Callback callback_;
};

bool CanRequestAutomation(const Extension* extension,
                          const AutomationInfo* automation_info,
                          const content::WebContents* contents) {
  if (automation_info->desktop)
    return true;

  const GURL& url = contents->GetURL();
  // TODO(aboxhall): check for webstore URL
  if (automation_info->matches.MatchesURL(url))
    return true;

  int tab_id = ExtensionTabUtil::GetTabId(contents);
  std::string unused_error;
  return extension->permissions_data()->CanAccessPage(extension, url, tab_id,
                                                      &unused_error);
}

}  // namespace

// Helper class that receives accessibility data from |WebContents|.
class AutomationWebContentsObserver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<AutomationWebContentsObserver> {
 public:
  ~AutomationWebContentsObserver() override {}

  // content::WebContentsObserver overrides.
  void AccessibilityEventReceived(
      const std::vector<content::AXEventNotificationDetails>& details)
      override {
    for (const auto& event : details) {
      ExtensionMsg_AccessibilityEventParams params;
      params.tree_id = event.ax_tree_id;
      params.id = event.id;
      params.event_type = event.event_type;
      params.update = event.update;
      params.event_from = event.event_from;
#if defined(USE_AURA)
      params.mouse_location = aura::Env::GetInstance()->last_mouse_location();
#endif

      AutomationEventRouter* router = AutomationEventRouter::GetInstance();
      router->DispatchAccessibilityEvent(params);
    }
  }

  void AccessibilityLocationChangesReceived(
      const std::vector<content::AXLocationChangeNotificationDetails>& details)
      override {
    for (const auto& src : details) {
      ExtensionMsg_AccessibilityLocationChangeParams dst;
      dst.id = src.id;
      dst.tree_id = src.ax_tree_id;
      dst.new_location = src.new_location;
      AutomationEventRouter* router = AutomationEventRouter::GetInstance();
      router->DispatchAccessibilityLocationChange(dst);
    }
  }

  void RenderFrameDeleted(
      content::RenderFrameHost* render_frame_host) override {
    int tree_id = render_frame_host->GetAXTreeID();
    AutomationEventRouter::GetInstance()->DispatchTreeDestroyedEvent(
        tree_id,
        browser_context_);
  }

  void MediaStartedPlaying(const MediaPlayerInfo& video_type,
                           const MediaPlayerId& id) override {
    std::vector<content::AXEventNotificationDetails> details;
    content::AXEventNotificationDetails detail;
    detail.ax_tree_id = id.first->GetAXTreeID();
    detail.event_type = ui::AX_EVENT_MEDIA_STARTED_PLAYING;
    details.push_back(detail);
    AccessibilityEventReceived(details);
  }

  void MediaStoppedPlaying(
      const MediaPlayerInfo& video_type,
      const MediaPlayerId& id,
      WebContentsObserver::MediaStoppedReason reason) override {
    std::vector<content::AXEventNotificationDetails> details;
    content::AXEventNotificationDetails detail;
    detail.ax_tree_id = id.first->GetAXTreeID();
    detail.event_type = ui::AX_EVENT_MEDIA_STOPPED_PLAYING;
    details.push_back(detail);
    AccessibilityEventReceived(details);
  }

 private:
  friend class content::WebContentsUserData<AutomationWebContentsObserver>;

  explicit AutomationWebContentsObserver(content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents),
        browser_context_(web_contents->GetBrowserContext()) {
    if (web_contents->WasRecentlyAudible()) {
      std::vector<content::AXEventNotificationDetails> details;
      content::RenderFrameHost* rfh = web_contents->GetMainFrame();
      if (!rfh)
        return;

      content::AXEventNotificationDetails detail;
      detail.ax_tree_id = rfh->GetAXTreeID();
      detail.event_type = ui::AX_EVENT_MEDIA_STARTED_PLAYING;
      details.push_back(detail);
      AccessibilityEventReceived(details);
    }
  }

  content::BrowserContext* browser_context_;

  DISALLOW_COPY_AND_ASSIGN(AutomationWebContentsObserver);
};

ExtensionFunction::ResponseAction
AutomationInternalEnableTabFunction::Run() {
  const AutomationInfo* automation_info = AutomationInfo::Get(extension());
  EXTENSION_FUNCTION_VALIDATE(automation_info);

  using api::automation_internal::EnableTab::Params;
  std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  content::WebContents* contents = NULL;
  if (params->args.tab_id.get()) {
    int tab_id = *params->args.tab_id;
    if (!ExtensionTabUtil::GetTabById(tab_id,
                                      GetProfile(),
                                      include_incognito(),
                                      NULL, /* browser out param*/
                                      NULL, /* tab_strip out param */
                                      &contents,
                                      NULL /* tab_index out param */)) {
      return RespondNow(
          Error(tabs_constants::kTabNotFoundError, base::IntToString(tab_id)));
    }
  } else {
    contents = GetCurrentBrowser()->tab_strip_model()->GetActiveWebContents();
    if (!contents)
      return RespondNow(Error("No active tab"));
  }

  content::RenderFrameHost* rfh = contents->GetMainFrame();
  if (!rfh)
    return RespondNow(Error("Could not enable accessibility for active tab"));

  if (!CanRequestAutomation(extension(), automation_info, contents)) {
    return RespondNow(
        Error(kCannotRequestAutomationOnPage, contents->GetURL().spec()));
  }

  AutomationWebContentsObserver::CreateForWebContents(contents);
  contents->EnableWebContentsOnlyAccessibilityMode();

  int ax_tree_id = rfh->GetAXTreeID();

  // This gets removed when the extension process dies.
  AutomationEventRouter::GetInstance()->RegisterListenerForOneTree(
      extension_id(),
      source_process_id(),
      params->args.routing_id,
      ax_tree_id);

  return RespondNow(ArgumentList(
      api::automation_internal::EnableTab::Results::Create(ax_tree_id)));
}

ExtensionFunction::ResponseAction AutomationInternalEnableFrameFunction::Run() {
  // TODO(dtseng): Limited to desktop tree for now pending out of proc iframes.
  using api::automation_internal::EnableFrame::Params;

  std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  content::RenderFrameHost* rfh =
      content::RenderFrameHost::FromAXTreeID(params->tree_id);
  if (!rfh)
    return RespondNow(Error("unable to load tab"));

  content::WebContents* contents =
      content::WebContents::FromRenderFrameHost(rfh);
  AutomationWebContentsObserver::CreateForWebContents(contents);

  // Only call this if this is the root of a frame tree, to avoid resetting
  // the accessibility state multiple times.
  if (!rfh->GetParent())
    contents->EnableWebContentsOnlyAccessibilityMode();

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
AutomationInternalPerformActionFunction::ConvertToAXActionData(
    api::automation_internal::PerformAction::Params* params,
    ui::AXActionData* action) {
  action->target_tree_id = params->args.tree_id;
  action->source_extension_id = extension_id();
  action->target_node_id = params->args.automation_node_id;
  int* request_id = params->args.request_id.get();
  action->request_id = request_id ? *request_id : -1;
  switch (params->args.action_type) {
    case api::automation_internal::ACTION_TYPE_DODEFAULT:
      action->action = ui::AX_ACTION_DO_DEFAULT;
      break;
    case api::automation_internal::ACTION_TYPE_FOCUS:
      action->action = ui::AX_ACTION_FOCUS;
      break;
    case api::automation_internal::ACTION_TYPE_GETIMAGEDATA: {
      api::automation_internal::GetImageDataParams get_image_data_params;
      EXTENSION_FUNCTION_VALIDATE(
          api::automation_internal::GetImageDataParams::Populate(
              params->opt_args.additional_properties, &get_image_data_params));
      action->action = ui::AX_ACTION_GET_IMAGE_DATA;
      action->target_rect = gfx::Rect(0, 0, get_image_data_params.max_width,
                                      get_image_data_params.max_height);
      break;
    }
    case api::automation_internal::ACTION_TYPE_HITTEST: {
      api::automation_internal::HitTestParams hit_test_params;
      EXTENSION_FUNCTION_VALIDATE(
          api::automation_internal::HitTestParams::Populate(
              params->opt_args.additional_properties, &hit_test_params));
      action->action = ui::AX_ACTION_HIT_TEST;
      action->target_point = gfx::Point(hit_test_params.x, hit_test_params.y);
      action->hit_test_event_to_fire =
          ui::ParseAXEvent(hit_test_params.event_to_fire);
      if (action->hit_test_event_to_fire == ui::AX_EVENT_NONE)
        return RespondNow(NoArguments());
      break;
    }
    case api::automation_internal::ACTION_TYPE_MAKEVISIBLE:
      action->action = ui::AX_ACTION_SCROLL_TO_MAKE_VISIBLE;
      break;
    case api::automation_internal::ACTION_TYPE_SCROLLBACKWARD:
      action->action = ui::AX_ACTION_SCROLL_BACKWARD;
      break;
    case api::automation_internal::ACTION_TYPE_SCROLLFORWARD:
      action->action = ui::AX_ACTION_SCROLL_FORWARD;
      break;
    case api::automation_internal::ACTION_TYPE_SCROLLUP:
      action->action = ui::AX_ACTION_SCROLL_UP;
      break;
    case api::automation_internal::ACTION_TYPE_SCROLLDOWN:
      action->action = ui::AX_ACTION_SCROLL_DOWN;
      break;
    case api::automation_internal::ACTION_TYPE_SCROLLLEFT:
      action->action = ui::AX_ACTION_SCROLL_LEFT;
      break;
    case api::automation_internal::ACTION_TYPE_SCROLLRIGHT:
      action->action = ui::AX_ACTION_SCROLL_RIGHT;
      break;
    case api::automation_internal::ACTION_TYPE_SETSELECTION: {
      api::automation_internal::SetSelectionParams selection_params;
      EXTENSION_FUNCTION_VALIDATE(
          api::automation_internal::SetSelectionParams::Populate(
              params->opt_args.additional_properties, &selection_params));
      action->anchor_node_id = params->args.automation_node_id;
      action->anchor_offset = selection_params.anchor_offset;
      action->focus_node_id = selection_params.focus_node_id;
      action->focus_offset = selection_params.focus_offset;
      action->action = ui::AX_ACTION_SET_SELECTION;
      break;
    }
    case api::automation_internal::ACTION_TYPE_SHOWCONTEXTMENU: {
      action->action = ui::AX_ACTION_SHOW_CONTEXT_MENU;
      break;
    }
    case api::automation_internal::
        ACTION_TYPE_SETSEQUENTIALFOCUSNAVIGATIONSTARTINGPOINT: {
      action->action =
          ui::AX_ACTION_SET_SEQUENTIAL_FOCUS_NAVIGATION_STARTING_POINT;
      break;
    }
    case api::automation_internal::ACTION_TYPE_CUSTOMACTION: {
      api::automation_internal::PerformCustomActionParams
          perform_custom_action_params;
      EXTENSION_FUNCTION_VALIDATE(
          api::automation_internal::PerformCustomActionParams::Populate(
              params->opt_args.additional_properties,
              &perform_custom_action_params));
      action->action = ui::AX_ACTION_CUSTOM_ACTION;
      action->custom_action_id = perform_custom_action_params.custom_action_id;
      break;
    }
    default:
      NOTREACHED();
  }
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
AutomationInternalPerformActionFunction::Run() {
  const AutomationInfo* automation_info = AutomationInfo::Get(extension());
  EXTENSION_FUNCTION_VALIDATE(automation_info && automation_info->interact);

  using api::automation_internal::PerformAction::Params;
  std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  ui::AXTreeIDRegistry* registry = ui::AXTreeIDRegistry::GetInstance();
  ui::AXHostDelegate* delegate =
      registry->GetHostDelegate(params->args.tree_id);
  if (delegate) {
#if defined(USE_AURA)
    ui::AXActionData data;
    ExtensionFunction::ResponseAction result =
        ConvertToAXActionData(params.get(), &data);
    delegate->PerformAction(data);
    return result;
#else
    NOTREACHED();
    return RespondNow(Error("Unexpected action on desktop automation tree;"
                            " platform does not support desktop automation"));
#endif  // defined(USE_AURA)
  }
  content::RenderFrameHost* rfh =
      content::RenderFrameHost::FromAXTreeID(params->args.tree_id);
  if (!rfh)
    return RespondNow(Error("Ignoring action on destroyed node"));

  content::WebContents* contents =
      content::WebContents::FromRenderFrameHost(rfh);
  if (!CanRequestAutomation(extension(), automation_info, contents)) {
    return RespondNow(
        Error(kCannotRequestAutomationOnPage, contents->GetURL().spec()));
  }

  // These actions are handled directly for the WebContents.
  if (params->args.action_type ==
      api::automation_internal::ACTION_TYPE_STARTDUCKINGMEDIA) {
    content::MediaSession* session = content::MediaSession::Get(contents);
    session->StartDucking();
    return RespondNow(NoArguments());
  } else if (params->args.action_type ==
             api::automation_internal::ACTION_TYPE_STOPDUCKINGMEDIA) {
    content::MediaSession* session = content::MediaSession::Get(contents);
    session->StopDucking();
    return RespondNow(NoArguments());
  } else if (params->args.action_type ==
             api::automation_internal::ACTION_TYPE_RESUMEMEDIA) {
    content::MediaSession* session = content::MediaSession::Get(contents);
    session->Resume(content::MediaSession::SuspendType::SYSTEM);
    return RespondNow(NoArguments());
  } else if (params->args.action_type ==
             api::automation_internal::ACTION_TYPE_SUSPENDMEDIA) {
    content::MediaSession* session = content::MediaSession::Get(contents);
    session->Suspend(content::MediaSession::SuspendType::SYSTEM);
    return RespondNow(NoArguments());
  }
  ui::AXActionData data;
  ExtensionFunction::ResponseAction result =
      ConvertToAXActionData(params.get(), &data);
  rfh->AccessibilityPerformAction(data);
  return result;
}

ExtensionFunction::ResponseAction
AutomationInternalEnableDesktopFunction::Run() {
#if defined(USE_AURA)
  const AutomationInfo* automation_info = AutomationInfo::Get(extension());
  if (!automation_info || !automation_info->desktop)
    return RespondNow(Error("desktop permission must be requested"));

  using api::automation_internal::EnableDesktop::Params;
  std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  // This gets removed when the extension process dies.
  AutomationEventRouter::GetInstance()->RegisterListenerWithDesktopPermission(
      extension_id(),
      source_process_id(),
      params->routing_id);

  AutomationManagerAura::GetInstance()->Enable(browser_context());
  return RespondNow(NoArguments());
#else
  return RespondNow(Error("getDesktop is unsupported by this platform"));
#endif  // defined(USE_AURA)
}

// static
int AutomationInternalQuerySelectorFunction::query_request_id_counter_ = 0;

ExtensionFunction::ResponseAction
AutomationInternalQuerySelectorFunction::Run() {
  const AutomationInfo* automation_info = AutomationInfo::Get(extension());
  EXTENSION_FUNCTION_VALIDATE(automation_info);

  using api::automation_internal::QuerySelector::Params;
  std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  content::RenderFrameHost* rfh =
      content::RenderFrameHost::FromAXTreeID(params->args.tree_id);
  if (!rfh) {
    return RespondNow(
        Error("domQuerySelector query sent on non-web or destroyed tree."));
  }

  content::WebContents* contents =
      content::WebContents::FromRenderFrameHost(rfh);

  int request_id = query_request_id_counter_++;
  base::string16 selector = base::UTF8ToUTF16(params->args.selector);

  // QuerySelectorHandler handles IPCs and deletes itself on completion.
  new QuerySelectorHandler(
      contents, request_id, params->args.automation_node_id, selector,
      base::Bind(&AutomationInternalQuerySelectorFunction::OnResponse, this));

  return RespondLater();
}

void AutomationInternalQuerySelectorFunction::OnResponse(
    const std::string& error,
    int result_acc_obj_id) {
  if (!error.empty()) {
    Respond(Error(error));
    return;
  }

  Respond(OneArgument(base::MakeUnique<base::Value>(result_acc_obj_id)));
}

}  // namespace extensions
