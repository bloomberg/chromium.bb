// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webui/remoting_ui.h"

#include "base/singleton.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/webui/chrome_url_data_manager.h"
#include "chrome/common/url_constants.h"
#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

///////////////////////////////////////////////////////////////////////////////
//
// RemotingHTMLSource
//
///////////////////////////////////////////////////////////////////////////////

class RemotingUIHTMLSource : public ChromeURLDataManager::DataSource {
 public:
  RemotingUIHTMLSource()
      : DataSource(chrome::kChromeUIRemotingHost, MessageLoop::current()) {}

  // Called when the network layer has requested a resource underneath
  // the path we registered.
  virtual void StartDataRequest(const std::string& path,
                                bool is_off_the_record,
                                int request_id);
  virtual std::string GetMimeType(const std::string&) const {
    return "pepper-application/x-chromoting";
  }

 private:
  ~RemotingUIHTMLSource() {}

  DISALLOW_COPY_AND_ASSIGN(RemotingUIHTMLSource);
};

void RemotingUIHTMLSource::StartDataRequest(const std::string& path,
                                           bool is_off_the_record,
                                           int request_id) {
  // Dummy data. Not used, but we need to send something back in the response.
  std::string full_html = "remoting";

  scoped_refptr<RefCountedBytes> html_bytes(new RefCountedBytes());
  html_bytes->data.resize(full_html.size());
  std::copy(full_html.begin(), full_html.end(), html_bytes->data.begin());

  SendResponse(request_id, html_bytes);
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
//
// RemotingUI
//
///////////////////////////////////////////////////////////////////////////////

RemotingUI::RemotingUI(TabContents* contents) : WebUI(contents) {
  RemotingUIHTMLSource* html_source = new RemotingUIHTMLSource();

  // Set up the chrome://remoting source.
  contents->profile()->GetChromeURLDataManager()->AddDataSource(html_source);
}


// static
RefCountedMemory* RemotingUI::GetFaviconResourceBytes() {
  return ResourceBundle::GetSharedInstance().
      // TODO(garykac): Have custom remoting icon created.
      LoadDataResourceBytes(IDR_PLUGIN);
}

// static
void RemotingUI::RegisterUserPrefs(PrefService* prefs) {
  // TODO(garykac): Add remoting prefs (if needed).
}
