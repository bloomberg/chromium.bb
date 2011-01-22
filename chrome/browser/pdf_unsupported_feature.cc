// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/pdf_unsupported_feature.h"

#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/plugin_service.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/tab_contents/infobar_delegate.h"
#include "chrome/browser/tab_contents/interstitial_page.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/jstemplate_builder.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "webkit/plugins/npapi/plugin_group.h"
#include "webkit/plugins/npapi/plugin_list.h"
#include "webkit/plugins/npapi/webplugininfo.h"

using webkit::npapi::PluginGroup;
using webkit::npapi::PluginList;
using webkit::npapi::WebPluginInfo;

// Only launch Adobe Reader X or later.
static const uint16 kMinReaderVersionToUse = 10;

namespace {

// Launch the url to get the latest Adbobe Reader installer.
void OpenReaderUpdateURL(TabContents* tab) {
  tab->OpenURL(GURL(PluginGroup::kAdobeReaderUpdateURL), GURL(), CURRENT_TAB,
               PageTransition::LINK);
}

// Opens the PDF using Adobe Reader.
void OpenUsingReader(TabContents* tab, const WebPluginInfo& reader_plugin) {
  PluginService::OverriddenPlugin plugin;
  plugin.render_process_id = tab->GetRenderProcessHost()->id();
  plugin.render_view_id = tab->render_view_host()->routing_id();
  plugin.url = tab->GetURL();
  plugin.plugin = reader_plugin;

  PluginService::GetInstance()->OverridePluginForTab(plugin);
  tab->render_view_host()->ReloadFrame();
}

// An interstitial to be used when the user chooses to open a PDF using Adobe
// Reader, but it is out of date.
class PDFUnsupportedFeatureInterstitial : public InterstitialPage {
 public:
  PDFUnsupportedFeatureInterstitial(
      TabContents* tab,
      const WebPluginInfo& reader_webplugininfo)
      : InterstitialPage(tab, false, tab->GetURL()),
        reader_webplugininfo_(reader_webplugininfo) {
  }

 protected:
  // InterstitialPage implementation.
  virtual std::string GetHTMLContents() {
    DictionaryValue strings;
    strings.SetString(
        "title",
        l10n_util::GetStringUTF16(IDS_READER_OUT_OF_DATE_BLOCKING_PAGE_TITLE));
    strings.SetString(
        "headLine",
        l10n_util::GetStringUTF16(IDS_READER_OUT_OF_DATE_BLOCKING_PAGE_BODY));
    strings.SetString(
        "update",
        l10n_util::GetStringUTF16(IDS_READER_OUT_OF_DATE_BLOCKING_PAGE_UPDATE));
    strings.SetString(
        "open_with_reader",
        l10n_util::GetStringUTF16(
            IDS_READER_OUT_OF_DATE_BLOCKING_PAGE_PROCEED));
    strings.SetString(
        "ok",
        l10n_util::GetStringUTF16(IDS_READER_OUT_OF_DATE_BLOCKING_PAGE_OK));
    strings.SetString(
        "cancel",
        l10n_util::GetStringUTF16(IDS_READER_OUT_OF_DATE_BLOCKING_PAGE_CANCEL));

    base::StringPiece html(ResourceBundle::GetSharedInstance().
        GetRawDataResource(IDR_READER_OUT_OF_DATE_HTML));

    return jstemplate_builder::GetI18nTemplateHtml(html, &strings);
  }

  virtual void CommandReceived(const std::string& command) {
    if (command == "0") {
      DontProceed();
      return;
    }

    if (command == "1") {
      OpenReaderUpdateURL(tab());
    } else if (command == "2") {
      OpenUsingReader(tab(), reader_webplugininfo_);
    } else {
      NOTREACHED();
    }
    Proceed();
  }

 private:
  WebPluginInfo reader_webplugininfo_;

  DISALLOW_COPY_AND_ASSIGN(PDFUnsupportedFeatureInterstitial);
};

// The info bar delegate used to inform the user that we don't support a feature
// in the PDF.
class PDFUnsupportedFeatureConfirmInfoBarDelegate
    : public ConfirmInfoBarDelegate {
 public:
  PDFUnsupportedFeatureConfirmInfoBarDelegate(
      TabContents* tab_contents,
      PluginGroup* reader_group)  // NULL if Adobe Reader isn't installed.
      : ConfirmInfoBarDelegate(tab_contents),
        tab_contents_(tab_contents),
        reader_installed_(!!reader_group),
        reader_vulnerable_(false) {
    if (reader_installed_) {
      std::vector<WebPluginInfo> plugins = reader_group->web_plugin_infos();
      DCHECK_EQ(plugins.size(), 1u);
      reader_webplugininfo_ = plugins[0];

      reader_vulnerable_ = reader_group->IsVulnerable();
      if (!reader_vulnerable_) {
        scoped_ptr<Version> version(PluginGroup::CreateVersionFromString(
            reader_webplugininfo_.version));
        if (version.get()) {
          if (version->components()[0] < kMinReaderVersionToUse)
            reader_vulnerable_ = true;
        }
      }
    }
  }

  // ConfirmInfoBarDelegate
  virtual void InfoBarClosed() {
    delete this;
  }
  virtual Type GetInfoBarType() {
    return PAGE_ACTION_TYPE;
  }
  virtual bool Accept() {
    LaunchReader();
    return true;
  }
  virtual int GetButtons() const {
    return BUTTON_OK | BUTTON_CANCEL;
  }
  virtual string16 GetButtonLabel(InfoBarButton button) const {
    switch (button) {
      case BUTTON_OK:
        return l10n_util::GetStringUTF16(
            IDS_CONFIRM_MESSAGEBOX_YES_BUTTON_LABEL);
      case BUTTON_CANCEL:
        return l10n_util::GetStringUTF16(
            IDS_CONFIRM_MESSAGEBOX_NO_BUTTON_LABEL);
      default:
        // All buttons are labeled above.
        NOTREACHED() << "Bad button id " << button;
        return string16();
    }
  }
  virtual string16 GetMessageText() const {
    return l10n_util::GetStringUTF16(reader_installed_ ?
        IDS_PDF_INFOBAR_QUESTION_READER_INSTALLED :
        IDS_PDF_INFOBAR_QUESTION_READER_NOT_INSTALLED);
  }

 private:
  void LaunchReader() {
    if (!reader_installed_) {
      OpenReaderUpdateURL(tab_contents_);
      return;
    }

    if (reader_vulnerable_) {
      PDFUnsupportedFeatureInterstitial* interstitial = new
          PDFUnsupportedFeatureInterstitial(
              tab_contents_, reader_webplugininfo_);
      interstitial->Show();
      return;
    }

    OpenUsingReader(tab_contents_, reader_webplugininfo_);
  }

  TabContents* tab_contents_;
  bool reader_installed_;
  bool reader_vulnerable_;
  WebPluginInfo reader_webplugininfo_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(PDFUnsupportedFeatureConfirmInfoBarDelegate);
};

}  // namespace

void PDFHasUnsupportedFeature(TabContents* tab) {
#if !defined(OS_WIN)
  // Only works for Windows for now.  For Mac, we'll have to launch the file
  // externally since Adobe Reader doesn't work inside Chrome.
  return;
#endif

  PluginGroup* reader_group = NULL;
  std::vector<PluginGroup> plugin_groups;
  PluginList::Singleton()->GetPluginGroups(
      false, &plugin_groups);
  string16 reader_group_name(UTF8ToUTF16(PluginGroup::kAdobeReaderGroupName));
  for (size_t i = 0; i < plugin_groups.size(); ++i) {
    if (plugin_groups[i].GetGroupName() == reader_group_name) {
      reader_group = &plugin_groups[i];
      break;
    }
  }

  // If the plugin is disabled by policy or by the user, don't prompt them.
  if (reader_group && !reader_group->Enabled())
    return;

  tab->AddInfoBar(new PDFUnsupportedFeatureConfirmInfoBarDelegate(
      tab, reader_group));
}
