// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/gdata_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chromeos/gdata_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_message_handler.h"

using content::WebUIMessageHandler;

// The handler for Javascript messages related to the "system" view.
class GDataHandler : public WebUIMessageHandler,
                     public base::SupportsWeakPtr<GDataHandler> {
 public:
  GDataHandler();
  virtual ~GDataHandler();

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(GDataHandler);
};

////////////////////////////////////////////////////////////////////////////////
//
// GDataHandler
//
////////////////////////////////////////////////////////////////////////////////
GDataHandler::GDataHandler() {
}

GDataHandler::~GDataHandler() {
}

void GDataHandler::RegisterMessages() {
  // TODO(zelidrag): add message registration, callbacks... if any...
}

////////////////////////////////////////////////////////////////////////////////
//
// GDataUI
//
////////////////////////////////////////////////////////////////////////////////

GDataUI::GDataUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  GDataHandler* handler = new GDataHandler();
  web_ui->AddMessageHandler(handler);
  Profile* profile = Profile::FromWebUI(web_ui);
  // Set up the chrome://gdata/ source.
  gdata::GDataSource* gdata_source = new gdata::GDataSource(profile);
  profile->GetChromeURLDataManager()->AddDataSource(gdata_source);
}
