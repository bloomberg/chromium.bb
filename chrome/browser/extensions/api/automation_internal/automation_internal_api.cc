// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/automation_internal/automation_internal_api.h"

#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/api/automation_internal.h"
#include "content/public/browser/ax_event_notification_details.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_system.h"
#include "ui/accessibility/ax_enums.h"
#include "ui/accessibility/ax_node_data.h"

namespace extensions {
class AutomationWebContentsObserver;
} // namespace extensions

DEFINE_WEB_CONTENTS_USER_DATA_KEY(extensions::AutomationWebContentsObserver);

namespace extensions {

using ui::AXNodeData;

namespace {

// Dispatches events to the extension message service.
void DispatchEvent(content::BrowserContext* context,
                   const std::string& event_name,
                   scoped_ptr<base::ListValue> args) {
  if (context && extensions::ExtensionSystem::Get(context)->event_router()) {
    scoped_ptr<Event> event(new Event(event_name, args.Pass()));
    event->restrict_to_browser_context = context;
    ExtensionSystem::Get(context)->event_router()->BroadcastEvent(event.Pass());
  }
}

}  // namespace

// Helper class that receives accessibility data from |WebContents|.
class AutomationWebContentsObserver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<AutomationWebContentsObserver> {
 public:
  virtual ~AutomationWebContentsObserver() {}

  // content::WebContentsObserver overrides.
  virtual void AccessibilityEventReceived(
      const std::vector<content::AXEventNotificationDetails>& details)
      OVERRIDE {
    if (!CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kEnableAutomationAPI)) {
      return;
    }

    using api::automation_internal::AXEventParams;

    std::vector<content::AXEventNotificationDetails>::const_iterator iter =
        details.begin();
    for (; iter != details.end(); ++iter) {
      const content::AXEventNotificationDetails& event = *iter;

      AXEventParams ax_tree_update;
      ax_tree_update.process_id = event.process_id;
      ax_tree_update.routing_id = event.routing_id;
      ax_tree_update.event_type = ToString(iter->event_type);

      for (size_t i = 0; i < event.nodes.size(); ++i) {
        linked_ptr<api::automation_internal::AXNodeData> out_node(
            new api::automation_internal::AXNodeData());
        PopulateNodeData(event.nodes[i], out_node);
        ax_tree_update.nodes.push_back(out_node);
      }

      // TODO(dtseng/aboxhall): Why are we sending only one update at a time? We
      // should match the behavior from renderer -> browser and send a
      // collection of tree updates over (to the extension); see
      // |AccessibilityHostMsg_EventParams| and |AccessibilityHostMsg_Events|.
      DispatchEvent(browser_context_,
          api::automation_internal::OnAccessibilityEvent::kEventName,
          api::automation_internal::OnAccessibilityEvent::Create(
              ax_tree_update));
    }
  }

 private:
  friend class content::WebContentsUserData<AutomationWebContentsObserver>;

  AutomationWebContentsObserver(
      content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents),
        browser_context_(web_contents->GetBrowserContext()) {}

  void PopulateNodeData(const ui::AXNodeData& node_data,
      linked_ptr<api::automation_internal::AXNodeData>& out_node_data) {
    out_node_data->id = node_data.id;
    out_node_data->role = ToString(node_data.role);

    uint32 state_pos = 0, state_shifter = node_data.state;
    while (state_shifter) {
      if (state_shifter & 1) {
        out_node_data->state.additional_properties.SetBoolean(
            ToString(static_cast<ui::AXState>(state_pos)), true);
      }
      state_shifter = state_shifter >> 1;
      state_pos++;
    }

    if (!node_data.bool_attributes.empty()) {
      out_node_data->bool_attributes.reset(
          new api::automation_internal::AXNodeData::BoolAttributes());
      for (size_t i = 0; i < node_data.bool_attributes.size(); ++i) {
        std::pair<ui::AXBoolAttribute, bool> attr =
            node_data.bool_attributes[i];
        out_node_data->bool_attributes->additional_properties.SetBoolean(
            ToString(attr.first), attr.second);
      }
    }

    if (!node_data.float_attributes.empty()) {
      out_node_data->float_attributes.reset(
          new api::automation_internal::AXNodeData::FloatAttributes());
      for (size_t i = 0; i < node_data.float_attributes.size(); ++i) {
        std::pair<ui::AXFloatAttribute, float> attr =
            node_data.float_attributes[i];
        out_node_data->float_attributes->additional_properties.SetDouble(
            ToString(attr.first), attr.second);
      }
    }

    if (!node_data.html_attributes.empty()) {
      out_node_data->html_attributes.reset(
          new api::automation_internal::AXNodeData::HtmlAttributes());
      for (size_t i = 0; i < node_data.html_attributes.size(); ++i) {
        std::pair<std::string, std::string> attr = node_data.html_attributes[i];
        out_node_data->html_attributes->additional_properties.SetString(
            attr.first, attr.second);
      }
    }

    if (!node_data.int_attributes.empty()) {
      out_node_data->int_attributes.reset(
          new api::automation_internal::AXNodeData::IntAttributes());
      for (size_t i = 0; i < node_data.int_attributes.size(); ++i) {
        std::pair<ui::AXIntAttribute, int> attr = node_data.int_attributes[i];
        out_node_data->int_attributes->additional_properties.SetInteger(
            ToString(attr.first), attr.second);
      }
    }

    if (!node_data.intlist_attributes.empty()) {
      out_node_data->intlist_attributes.reset(
          new api::automation_internal::AXNodeData::IntlistAttributes());
      for (size_t i = 0; i < node_data.intlist_attributes.size(); ++i) {
        std::pair<ui::AXIntListAttribute, std::vector<int32> > attr =
            node_data.intlist_attributes[i];
        base::ListValue* intlist = new base::ListValue();
        for (size_t j = 0; j < attr.second.size(); ++j)
          intlist->AppendInteger(attr.second[j]);
        out_node_data->intlist_attributes->additional_properties.Set(
            ToString(attr.first), intlist);
      }
    }

    if (!node_data.string_attributes.empty()) {
      out_node_data->string_attributes.reset(
          new api::automation_internal::AXNodeData::StringAttributes());
      for (size_t i = 0; i < node_data.string_attributes.size(); ++i) {
        std::pair<ui::AXStringAttribute, std::string> attr =
            node_data.string_attributes[i];
        out_node_data->string_attributes->additional_properties.SetString(
            ToString(attr.first), attr.second);
      }
    }

    for (size_t i = 0; i < node_data.child_ids.size(); ++i) {
      out_node_data->child_ids.push_back(node_data.child_ids[i]);
    }
  }

  content::BrowserContext* browser_context_;

  DISALLOW_COPY_AND_ASSIGN(AutomationWebContentsObserver);
};

// TODO(aboxhall/dtseng): ensure that the initial data is sent down for the tab
// if this doesn't turn accessibility on for the first time (e.g. if a
// RendererAccessibility object existed already because a screenreader has been
// run at some point).
bool AutomationInternalEnableCurrentTabFunction::RunImpl() {
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableAutomationAPI)) {
    return false;
  }

  Browser* current_browser = GetCurrentBrowser();
  TabStripModel* tab_strip = current_browser->tab_strip_model();
  content::WebContents* contents =
      tab_strip->GetWebContentsAt(tab_strip->active_index());
  if (!contents)
    return false;
  content::RenderWidgetHost* rwh =
      contents->GetRenderWidgetHostView()->GetRenderWidgetHost();
  if (!rwh)
    return false;

  AutomationWebContentsObserver::CreateForWebContents(contents);

  rwh->EnableTreeOnlyAccessibilityMode();

  results_ = api::automation_internal::EnableCurrentTab::Results::Create(
      rwh->GetProcess()->GetID(), rwh->GetRoutingID());

  SendResponse(true);
  return true;
}

bool AutomationInternalPerformActionFunction::RunImpl() {
  using api::automation_internal::PerformAction::Params;
  scoped_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  content::RenderWidgetHost* rwh =
      content::RenderWidgetHost::FromID(params->args.process_id,
                                        params->args.routing_id);

  switch (params->args.action_type) {
    case api::automation_internal::ACTION_TYPE_DO_DEFAULT:
      rwh->AccessibilityDoDefaultAction(params->args.automation_node_id);
      break;
    case api::automation_internal::ACTION_TYPE_FOCUS:
      rwh->AccessibilitySetFocus(params->args.automation_node_id);
      break;
    case api::automation_internal::ACTION_TYPE_MAKE_VISIBLE:
      rwh->AccessibilityScrollToMakeVisible(params->args.automation_node_id,
                                            gfx::Rect());
      break;
    case api::automation_internal::ACTION_TYPE_SET_SELECTION: {
      extensions::api::automation_internal::SetSelectionParams selection_params;
      EXTENSION_FUNCTION_VALIDATE(
          extensions::api::automation_internal::SetSelectionParams::Populate(
              params->opt_args.additional_properties, &selection_params));
      rwh->AccessibilitySetTextSelection(params->args.automation_node_id,
                                         selection_params.start_index,
                                         selection_params.end_index);
      break;
    }
    default:
      NOTREACHED();
  }
  return true;
}

}  // namespace extensions
