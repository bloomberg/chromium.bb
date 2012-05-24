// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/pdf/pdf_unsupported_feature.h"

#include "base/bind.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/chrome_plugin_service_filter.h"
#include "chrome/browser/infobars/infobar_tab_helper.h"
#include "chrome/browser/plugin_prefs.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_preferences_util.h"
#include "chrome/browser/tab_contents/confirm_infobar_delegate.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/interstitial_page.h"
#include "content/public/browser/interstitial_page_delegate.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources_standard.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/layout.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "webkit/plugins/npapi/plugin_group.h"

using content::InterstitialPage;
using content::OpenURLParams;
using content::PluginService;
using content::Referrer;
using content::UserMetricsAction;
using content::WebContents;
using webkit::npapi::PluginGroup;
using webkit::WebPluginInfo;

namespace {

// Only launch Adobe Reader X or later.
static const uint16 kMinReaderVersionToUse = 10;

static const char kReaderUpdateUrl[] =
    "http://www.adobe.com/go/getreader_chrome";

// The info bar delegate used to ask the user if they want to use Adobe Reader
// by default.  We want the infobar to have [No][Yes], so we swap the text on
// the buttons, and the meaning of the delegate callbacks.
class PDFEnableAdobeReaderInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  explicit PDFEnableAdobeReaderInfoBarDelegate(
      InfoBarTabHelper* infobar_helper,
      Profile* profile);
  virtual ~PDFEnableAdobeReaderInfoBarDelegate();

  // ConfirmInfoBarDelegate
  virtual void InfoBarDismissed() OVERRIDE;
  virtual Type GetInfoBarType() const OVERRIDE;
  virtual bool Accept() OVERRIDE;
  virtual bool Cancel() OVERRIDE;
  virtual string16 GetButtonLabel(InfoBarButton button) const OVERRIDE;
  virtual string16 GetMessageText() const OVERRIDE;

 private:
  void OnYes();
  void OnNo();

  Profile* profile_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(PDFEnableAdobeReaderInfoBarDelegate);
};

PDFEnableAdobeReaderInfoBarDelegate::PDFEnableAdobeReaderInfoBarDelegate(
    InfoBarTabHelper* infobar_helper, Profile* profile)
    : ConfirmInfoBarDelegate(infobar_helper),
      profile_(profile) {
  content::RecordAction(UserMetricsAction("PDF_EnableReaderInfoBarShown"));
}

PDFEnableAdobeReaderInfoBarDelegate::~PDFEnableAdobeReaderInfoBarDelegate() {
}

void PDFEnableAdobeReaderInfoBarDelegate::InfoBarDismissed() {
  OnNo();
}

InfoBarDelegate::Type
    PDFEnableAdobeReaderInfoBarDelegate::GetInfoBarType() const {
  return PAGE_ACTION_TYPE;
}

bool PDFEnableAdobeReaderInfoBarDelegate::Accept() {
  profile_->GetPrefs()->SetBoolean(
      prefs::kPluginsShowSetReaderDefaultInfobar, false);
  OnNo();
  return true;
}

bool PDFEnableAdobeReaderInfoBarDelegate::Cancel() {
  OnYes();
  return true;
}

string16 PDFEnableAdobeReaderInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16((button == BUTTON_OK) ?
      IDS_PDF_INFOBAR_NEVER_USE_READER_BUTTON :
      IDS_PDF_INFOBAR_ALWAYS_USE_READER_BUTTON);
}

string16 PDFEnableAdobeReaderInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringUTF16(IDS_PDF_INFOBAR_QUESTION_ALWAYS_USE_READER);
}

void PDFEnableAdobeReaderInfoBarDelegate::OnYes() {
  content::RecordAction(UserMetricsAction("PDF_EnableReaderInfoBarOK"));
  PluginPrefs* plugin_prefs = PluginPrefs::GetForProfile(profile_);
  plugin_prefs->EnablePluginGroup(
      true, ASCIIToUTF16(webkit::npapi::PluginGroup::kAdobeReaderGroupName));
  plugin_prefs->EnablePluginGroup(
      false, ASCIIToUTF16(chrome::ChromeContentClient::kPDFPluginName));
}

void PDFEnableAdobeReaderInfoBarDelegate::OnNo() {
  content::RecordAction(UserMetricsAction("PDF_EnableReaderInfoBarCancel"));
}

// Launch the url to get the latest Adbobe Reader installer.
void OpenReaderUpdateURL(WebContents* tab) {
  OpenURLParams params(
      GURL(kReaderUpdateUrl), Referrer(), NEW_FOREGROUND_TAB,
      content::PAGE_TRANSITION_LINK, false);
  tab->OpenURL(params);
}

// Opens the PDF using Adobe Reader.
void OpenUsingReader(TabContentsWrapper* tab,
                     const WebPluginInfo& reader_plugin,
                     InfoBarDelegate* old_delegate,
                     InfoBarDelegate* new_delegate) {
  ChromePluginServiceFilter::GetInstance()->OverridePluginForTab(
      tab->web_contents()->GetRenderProcessHost()->GetID(),
      tab->web_contents()->GetRenderViewHost()->GetRoutingID(),
      tab->web_contents()->GetURL(),
      ASCIIToUTF16(PluginGroup::kAdobeReaderGroupName));
  tab->web_contents()->GetRenderViewHost()->ReloadFrame();

  if (new_delegate) {
    if (old_delegate) {
      tab->infobar_tab_helper()->ReplaceInfoBar(old_delegate, new_delegate);
    } else {
      tab->infobar_tab_helper()->AddInfoBar(new_delegate);
    }
  }
}

// An interstitial to be used when the user chooses to open a PDF using Adobe
// Reader, but it is out of date.
class PDFUnsupportedFeatureInterstitial
    : public content::InterstitialPageDelegate {
 public:
  PDFUnsupportedFeatureInterstitial(
      TabContentsWrapper* tab,
      const WebPluginInfo& reader_webplugininfo)
      : tab_contents_(tab),
        reader_webplugininfo_(reader_webplugininfo) {
    content::RecordAction(UserMetricsAction("PDF_ReaderInterstitialShown"));
    interstitial_page_ = InterstitialPage::Create(
        tab->web_contents(), false, tab->web_contents()->GetURL(), this);
    interstitial_page_->Show();
  }

 protected:
  // InterstitialPageDelegate implementation.
  virtual std::string GetHTMLContents() OVERRIDE {
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
        GetRawDataResource(IDR_READER_OUT_OF_DATE_HTML,
                           ui::SCALE_FACTOR_NONE));

    return jstemplate_builder::GetI18nTemplateHtml(html, &strings);
  }

  virtual void CommandReceived(const std::string& command) OVERRIDE {
    if (command == "0") {
      content::RecordAction(
          UserMetricsAction("PDF_ReaderInterstitialCancel"));
      interstitial_page_->DontProceed();
      return;
    }

    if (command == "1") {
      content::RecordAction(
          UserMetricsAction("PDF_ReaderInterstitialUpdate"));
      OpenReaderUpdateURL(tab_contents_->web_contents());
    } else if (command == "2") {
      content::RecordAction(
          UserMetricsAction("PDF_ReaderInterstitialIgnore"));
      OpenUsingReader(tab_contents_, reader_webplugininfo_, NULL, NULL);
    } else {
      NOTREACHED();
    }
    interstitial_page_->Proceed();
  }

  virtual void OverrideRendererPrefs(
      content::RendererPreferences* prefs) OVERRIDE {
    renderer_preferences_util::UpdateFromSystemSettings(
        prefs, tab_contents_->profile());
  }

 private:
  TabContentsWrapper* tab_contents_;
  WebPluginInfo reader_webplugininfo_;
  InterstitialPage* interstitial_page_;  // Owns us.

  DISALLOW_COPY_AND_ASSIGN(PDFUnsupportedFeatureInterstitial);
};

// The info bar delegate used to inform the user that we don't support a feature
// in the PDF.  See the comment about how we swap buttons for
// PDFEnableAdobeReaderInfoBarDelegate.
class PDFUnsupportedFeatureInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  // |reader_group| is NULL if Adobe Reader isn't installed.
  PDFUnsupportedFeatureInfoBarDelegate(TabContentsWrapper* tab_contents,
                                       const PluginGroup* reader_group);
  virtual ~PDFUnsupportedFeatureInfoBarDelegate();

  // ConfirmInfoBarDelegate
  virtual void InfoBarDismissed() OVERRIDE;
  virtual gfx::Image* GetIcon() const OVERRIDE;
  virtual Type GetInfoBarType() const OVERRIDE;
  virtual bool Accept() OVERRIDE;
  virtual bool Cancel() OVERRIDE;
  virtual string16 GetButtonLabel(InfoBarButton button) const OVERRIDE;
  virtual string16 GetMessageText() const OVERRIDE;

 private:
  bool OnYes();
  void OnNo();

  TabContentsWrapper* tab_contents_;
  bool reader_installed_;
  bool reader_vulnerable_;
  WebPluginInfo reader_webplugininfo_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(PDFUnsupportedFeatureInfoBarDelegate);
};

PDFUnsupportedFeatureInfoBarDelegate::PDFUnsupportedFeatureInfoBarDelegate(
    TabContentsWrapper* tab_contents,
    const PluginGroup* reader_group)
    : ConfirmInfoBarDelegate(tab_contents->infobar_tab_helper()),
      tab_contents_(tab_contents),
      reader_installed_(!!reader_group),
      reader_vulnerable_(false) {
  if (!reader_installed_) {
    content::RecordAction(
        UserMetricsAction("PDF_InstallReaderInfoBarShown"));
    return;
  }

  content::RecordAction(UserMetricsAction("PDF_UseReaderInfoBarShown"));
  const std::vector<WebPluginInfo>& plugins =
      reader_group->web_plugin_infos();
  DCHECK_EQ(plugins.size(), 1u);
  reader_webplugininfo_ = plugins[0];

  reader_vulnerable_ = reader_group->IsVulnerable(reader_webplugininfo_);
  if (!reader_vulnerable_) {
    scoped_ptr<Version> version(PluginGroup::CreateVersionFromString(
        reader_webplugininfo_.version));
    reader_vulnerable_ =
        version.get() && (version->components()[0] < kMinReaderVersionToUse);
  }
}

PDFUnsupportedFeatureInfoBarDelegate::~PDFUnsupportedFeatureInfoBarDelegate() {
}

void PDFUnsupportedFeatureInfoBarDelegate::InfoBarDismissed() {
  OnNo();
}

gfx::Image* PDFUnsupportedFeatureInfoBarDelegate::GetIcon() const {
  return &ResourceBundle::GetSharedInstance().GetNativeImageNamed(
      IDR_INFOBAR_INCOMPLETE);
}

InfoBarDelegate::Type
    PDFUnsupportedFeatureInfoBarDelegate::GetInfoBarType() const {
  return PAGE_ACTION_TYPE;
}

bool PDFUnsupportedFeatureInfoBarDelegate::Accept() {
  OnNo();
  return true;
}

bool PDFUnsupportedFeatureInfoBarDelegate::Cancel() {
  return OnYes();
}

string16 PDFUnsupportedFeatureInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16((button == BUTTON_OK) ?
      IDS_CONFIRM_MESSAGEBOX_NO_BUTTON_LABEL :
      IDS_CONFIRM_MESSAGEBOX_YES_BUTTON_LABEL);
}

string16 PDFUnsupportedFeatureInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringUTF16(reader_installed_ ?
      IDS_PDF_INFOBAR_QUESTION_READER_INSTALLED :
      IDS_PDF_INFOBAR_QUESTION_READER_NOT_INSTALLED);
}

bool PDFUnsupportedFeatureInfoBarDelegate::OnYes() {
  if (!reader_installed_) {
    content::RecordAction(UserMetricsAction("PDF_InstallReaderInfoBarOK"));
    OpenReaderUpdateURL(tab_contents_->web_contents());
    return true;
  }

  content::RecordAction(UserMetricsAction("PDF_UseReaderInfoBarOK"));

  if (reader_vulnerable_) {
    new PDFUnsupportedFeatureInterstitial(tab_contents_, reader_webplugininfo_);
    return true;
  }

  if (tab_contents_->profile()->GetPrefs()->GetBoolean(
      prefs::kPluginsShowSetReaderDefaultInfobar)) {
    InfoBarDelegate* bar = new PDFEnableAdobeReaderInfoBarDelegate(
        tab_contents_->infobar_tab_helper(), tab_contents_->profile());
    OpenUsingReader(tab_contents_, reader_webplugininfo_, this, bar);
    return false;
  }

  OpenUsingReader(tab_contents_, reader_webplugininfo_, NULL, NULL);
  return true;
}

void PDFUnsupportedFeatureInfoBarDelegate::OnNo() {
  content::RecordAction(reader_installed_ ?
                        UserMetricsAction("PDF_UseReaderInfoBarCancel") :
                        UserMetricsAction("PDF_InstallReaderInfoBarCancel"));
}

void GotPluginGroupsCallback(int process_id,
                             int routing_id,
                             const std::vector<PluginGroup>& groups) {
  WebContents* web_contents =
      tab_util::GetWebContentsByID(process_id, routing_id);
  if (!web_contents)
    return;

  TabContentsWrapper* tab =
      TabContentsWrapper::GetCurrentWrapperForContents(web_contents);
  if (!tab)
    return;

  string16 reader_group_name(ASCIIToUTF16(PluginGroup::kAdobeReaderGroupName));

  // If the Reader plugin is disabled by policy, don't prompt them.
  PluginPrefs* plugin_prefs = PluginPrefs::GetForProfile(tab->profile());
  if (plugin_prefs->PolicyStatusForPlugin(reader_group_name) ==
      PluginPrefs::POLICY_DISABLED) {
    return;
  }

  const PluginGroup* reader_group = NULL;
  for (size_t i = 0; i < groups.size(); ++i) {
    if (groups[i].GetGroupName() == reader_group_name) {
      reader_group = &groups[i];
      break;
    }
  }

  tab->infobar_tab_helper()->AddInfoBar(
      new PDFUnsupportedFeatureInfoBarDelegate(tab, reader_group));
}

}  // namespace

void PDFHasUnsupportedFeature(TabContentsWrapper* tab) {
#if !defined(OS_WIN)
  // Only works for Windows for now.  For Mac, we'll have to launch the file
  // externally since Adobe Reader doesn't work inside Chrome.
  return;
#endif

  PluginService::GetInstance()->GetPluginGroups(
      base::Bind(&GotPluginGroupsCallback,
          tab->web_contents()->GetRenderProcessHost()->GetID(),
          tab->web_contents()->GetRenderViewHost()->GetRoutingID()));
}
