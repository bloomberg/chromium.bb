// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/tracking_ui.h"

#include <string>

#include "base/bind.h"
#include "base/tracked_objects.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chrome_web_ui_data_source.h"
#include "chrome/common/url_constants.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/trace_controller.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"

using content::BrowserThread;

namespace {

ChromeWebUIDataSource* CreateTrackingHTMLSource() {
  // TODO(eroman): Use kChromeUITrackingHost instead of kChromeUITrackingHost2
  //               once migration to webui is complete.
  ChromeWebUIDataSource* source =
      new ChromeWebUIDataSource(chrome::kChromeUITrackingHost2);

  source->set_json_path("strings.js");
  source->add_resource_path("tracking.js", IDR_TRACKING_JS);
  source->set_default_resource(IDR_TRACKING_HTML);
  return source;
}

// This class receives javascript messages from the renderer.
// Note that the WebUI infrastructure runs on the UI thread, therefore all of
// this class's methods are expected to run on the UI thread.
class TrackingMessageHandler : public WebUIMessageHandler {
 public:
  TrackingMessageHandler() {}

  // WebUIMessageHandler implementation.
  virtual WebUIMessageHandler* Attach(WebUI* web_ui) OVERRIDE;
  virtual void RegisterMessages() OVERRIDE;

  // Messages.
  void OnGetData(const ListValue* list);
  void OnResetData(const ListValue* list);

 private:
  DISALLOW_COPY_AND_ASSIGN(TrackingMessageHandler);
};

WebUIMessageHandler* TrackingMessageHandler::Attach(WebUI* web_ui) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  WebUIMessageHandler* result = WebUIMessageHandler::Attach(web_ui);
  return result;
}

void TrackingMessageHandler::RegisterMessages() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  web_ui_->RegisterMessageCallback("getData",
      base::Bind(&TrackingMessageHandler::OnGetData,base::Unretained(this)));
  web_ui_->RegisterMessageCallback("resetData",
      base::Bind(&TrackingMessageHandler::OnResetData,
                 base::Unretained(this)));
}

void TrackingMessageHandler::OnGetData(const ListValue* list) {
  // TODO(eroman): Gather all the task tracking information into a Value.
  ListValue values;

  // Send the data to the renderer.
  web_ui_->CallJavascriptFunction("g_browserBridge.receivedData", values);
}

void TrackingMessageHandler::OnResetData(const ListValue* list) {
  tracked_objects::ThreadData::ResetAllThreadData();
}

}  // namespace

TrackingUI::TrackingUI(TabContents* contents) : ChromeWebUI(contents) {
  AddMessageHandler((new TrackingMessageHandler())->Attach(this));

  // Set up the chrome://tracking2/ source.
  Profile::FromBrowserContext(contents->browser_context())->
      GetChromeURLDataManager()->AddDataSource(CreateTrackingHTMLSource());
}
