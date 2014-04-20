// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/automation_internal/automation_internal_api.h"

#include <vector>

#include "base/command_line.h"
#include "chrome/browser/extensions/api/automation_internal/automation_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/api/automation_internal.h"
#include "content/public/browser/ax_event_notification_details.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"

namespace extensions {
class AutomationWebContentsObserver;
}  // namespace extensions

DEFINE_WEB_CONTENTS_USER_DATA_KEY(extensions::AutomationWebContentsObserver);

namespace extensions {

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
    automation_util::DispatchAccessibilityEventsToAutomation(
        details, browser_context_);
  }

 private:
  friend class content::WebContentsUserData<AutomationWebContentsObserver>;

  AutomationWebContentsObserver(
      content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents),
        browser_context_(web_contents->GetBrowserContext()) {}

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

  results_ = api::automation_internal::EnableCurrentTab::Results::Create(
      rwh->GetProcess()->GetID(), rwh->GetRoutingID());

  SendResponse(true);

  AutomationWebContentsObserver::CreateForWebContents(contents);

  rwh->EnableTreeOnlyAccessibilityMode();

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
