// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/media/media_internals_ui.h"

#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/browser/ui/webui/media/media_internals_handler.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/url_constants.h"
#include "grit/browser_resources.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

class MediaInternalsHTMLSource : public ChromeURLDataManager::DataSource {
 public:
  MediaInternalsHTMLSource();

  // Called when the network layer has requested a resource underneath
  // the path we registered.
  virtual void StartDataRequest(const std::string& path,
                                bool is_incognito,
                                int request_id);
  virtual std::string GetMimeType(const std::string& path) const;

 private:
  virtual ~MediaInternalsHTMLSource() {}
  DISALLOW_COPY_AND_ASSIGN(MediaInternalsHTMLSource);
};

////////////////////////////////////////////////////////////////////////////////
//
// MediaInternalsHTMLSource
//
////////////////////////////////////////////////////////////////////////////////

MediaInternalsHTMLSource::MediaInternalsHTMLSource()
    : DataSource(chrome::kChromeUIMediaInternalsHost, MessageLoop::current()) {
}

void MediaInternalsHTMLSource::StartDataRequest(const std::string& path,
                                                bool is_incognito,
                                                int request_id) {
  DictionaryValue localized_strings;
  SetFontAndTextDirection(&localized_strings);

  base::StringPiece html_template(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_MEDIA_INTERNALS_HTML));
  std::string html(html_template.data(), html_template.size());
  jstemplate_builder::AppendI18nTemplateSourceHtml(&html);
  jstemplate_builder::AppendJsTemplateSourceHtml(&html);
  jstemplate_builder::AppendJsonHtml(&localized_strings, &html);
  jstemplate_builder::AppendI18nTemplateProcessHtml(&html);

  SendResponse(request_id, base::RefCountedString::TakeString(&html));
}

std::string MediaInternalsHTMLSource::GetMimeType(
    const std::string& path) const {
  return "text/html";
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
//
// MediaInternalsUI
//
////////////////////////////////////////////////////////////////////////////////

MediaInternalsUI::MediaInternalsUI(TabContents* contents)
    : ChromeWebUI(contents) {
  AddMessageHandler((new MediaInternalsMessageHandler())->Attach(this));

  Profile* profile = Profile::FromBrowserContext(contents->browser_context());
  profile->GetChromeURLDataManager()->AddDataSource(
      new MediaInternalsHTMLSource());
}
