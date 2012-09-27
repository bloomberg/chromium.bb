// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/pdf/pdf_unsupported_feature.h"

#include "base/bind.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/api/infobars/confirm_infobar_delegate.h"
#include "chrome/browser/chrome_plugin_service_filter.h"
#include "chrome/browser/infobars/infobar_tab_helper.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/plugins/plugin_finder.h"
#include "chrome/browser/plugins/plugin_metadata.h"
#include "chrome/browser/plugins/plugin_prefs.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_preferences_util.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/pdf/open_pdf_in_reader_prompt_delegate.h"
#include "chrome/browser/ui/pdf/pdf_tab_helper.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
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
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/layout.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "webkit/plugins/npapi/plugin_group.h"

#if defined(OS_WIN)
#include "base/win/metro.h"
#endif

using content::InterstitialPage;
using content::OpenURLParams;
using content::PluginService;
using content::Referrer;
using content::UserMetricsAction;
using content::WebContents;
using webkit::npapi::PluginGroup;
using webkit::WebPluginInfo;

namespace {

static const char kAdobeReaderIdentifier[] = "adobe-reader";
static const char kAdobeReaderUpdateUrl[] =
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
      GURL(kAdobeReaderUpdateUrl), Referrer(), NEW_FOREGROUND_TAB,
      content::PAGE_TRANSITION_LINK, false);
  tab->OpenURL(params);
}

// Opens the PDF using Adobe Reader.
void OpenUsingReader(TabContents* tab,
                     const WebPluginInfo& reader_plugin,
                     InfoBarDelegate* delegate) {
  ChromePluginServiceFilter::GetInstance()->OverridePluginForTab(
      tab->web_contents()->GetRenderProcessHost()->GetID(),
      tab->web_contents()->GetRenderViewHost()->GetRoutingID(),
      tab->web_contents()->GetURL(),
      reader_plugin);
  tab->web_contents()->GetRenderViewHost()->ReloadFrame();

  if (delegate)
    tab->infobar_tab_helper()->AddInfoBar(delegate);
}

// An interstitial to be used when the user chooses to open a PDF using Adobe
// Reader, but it is out of date.
class PDFUnsupportedFeatureInterstitial
    : public content::InterstitialPageDelegate {
 public:
  PDFUnsupportedFeatureInterstitial(
      TabContents* tab,
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
      // Pretend that the plug-in is up-to-date so that we don't block it.
      reader_webplugininfo_.version = ASCIIToUTF16("11.0.0.0");
      OpenUsingReader(tab_contents_, reader_webplugininfo_, NULL);
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
  TabContents* tab_contents_;
  WebPluginInfo reader_webplugininfo_;
  InterstitialPage* interstitial_page_;  // Owns us.

  DISALLOW_COPY_AND_ASSIGN(PDFUnsupportedFeatureInterstitial);
};

// The delegate for the bubble used to inform the user that we don't support a
// feature in the PDF.
class PDFUnsupportedFeaturePromptDelegate
    : public OpenPDFInReaderPromptDelegate {
 public:
  // |reader| is NULL if Adobe Reader isn't installed.
  PDFUnsupportedFeaturePromptDelegate(TabContents* tab_contents,
                                      const webkit::WebPluginInfo* reader,
                                      PluginFinder* plugin_finder);
  virtual ~PDFUnsupportedFeaturePromptDelegate();

  // OpenPDFInReaderPromptDelegate:
  virtual string16 GetMessageText() const OVERRIDE;
  virtual string16 GetAcceptButtonText() const OVERRIDE;
  virtual string16 GetCancelButtonText() const OVERRIDE;
  virtual void Accept() OVERRIDE;
  virtual void Cancel() OVERRIDE;

 private:
  TabContents* tab_contents_;
  bool reader_installed_;
  bool reader_vulnerable_;
  WebPluginInfo reader_webplugininfo_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(PDFUnsupportedFeaturePromptDelegate);
};

PDFUnsupportedFeaturePromptDelegate::PDFUnsupportedFeaturePromptDelegate(
    TabContents* tab_contents,
    const webkit::WebPluginInfo* reader,
    PluginFinder* plugin_finder)
    : tab_contents_(tab_contents),
      reader_installed_(!!reader),
      reader_vulnerable_(false) {
  if (!reader_installed_) {
    content::RecordAction(
        UserMetricsAction("PDF_InstallReaderInfoBarShown"));
    return;
  }

  content::RecordAction(UserMetricsAction("PDF_UseReaderInfoBarShown"));
  reader_webplugininfo_ = *reader;

#if defined(ENABLE_PLUGIN_INSTALLATION)
  PluginMetadata* plugin_metadata =
      plugin_finder->GetPluginMetadata(reader_webplugininfo_);

  reader_vulnerable_ = plugin_metadata->GetSecurityStatus(*reader) !=
                       PluginMetadata::SECURITY_STATUS_UP_TO_DATE;
#else
  NOTREACHED();
#endif
}

PDFUnsupportedFeaturePromptDelegate::~PDFUnsupportedFeaturePromptDelegate() {
}

string16 PDFUnsupportedFeaturePromptDelegate::GetMessageText() const {
  return l10n_util::GetStringUTF16(IDS_PDF_BUBBLE_MESSAGE);
}

string16 PDFUnsupportedFeaturePromptDelegate::GetAcceptButtonText() const {
#if defined(OS_WIN)
  if (base::win::IsMetroProcess())
    return l10n_util::GetStringUTF16(IDS_PDF_BUBBLE_METRO_MODE_LINK);
#endif

  if (reader_installed_)
    return l10n_util::GetStringUTF16(IDS_PDF_BUBBLE_OPEN_IN_READER_LINK);

  return l10n_util::GetStringUTF16(IDS_PDF_BUBBLE_INSTALL_READER_LINK);
}

string16 PDFUnsupportedFeaturePromptDelegate::GetCancelButtonText() const {
  return l10n_util::GetStringUTF16(IDS_DONE);
}

void PDFUnsupportedFeaturePromptDelegate::Accept() {
#if defined(OS_WIN)
  if (base::win::IsMetroProcess()) {
    browser::AttemptRestartWithModeSwitch();
    return;
  }
#endif

  if (!reader_installed_) {
    content::RecordAction(UserMetricsAction("PDF_InstallReaderInfoBarOK"));
    OpenReaderUpdateURL(tab_contents_->web_contents());
    return;
  }

  content::RecordAction(UserMetricsAction("PDF_UseReaderInfoBarOK"));

  if (reader_vulnerable_) {
    new PDFUnsupportedFeatureInterstitial(tab_contents_, reader_webplugininfo_);
    return;
  }

  if (tab_contents_->profile()->GetPrefs()->GetBoolean(
      prefs::kPluginsShowSetReaderDefaultInfobar)) {
    InfoBarDelegate* bar = new PDFEnableAdobeReaderInfoBarDelegate(
        tab_contents_->infobar_tab_helper(), tab_contents_->profile());
    OpenUsingReader(tab_contents_, reader_webplugininfo_, bar);
    return;
  }

  OpenUsingReader(tab_contents_, reader_webplugininfo_, NULL);
}

void PDFUnsupportedFeaturePromptDelegate::Cancel() {
  content::RecordAction(reader_installed_ ?
                        UserMetricsAction("PDF_UseReaderInfoBarCancel") :
                        UserMetricsAction("PDF_InstallReaderInfoBarCancel"));
}

void GotPluginsCallback(int process_id,
                        int routing_id,
                        const std::vector<webkit::WebPluginInfo>& plugins) {
  WebContents* web_contents =
      tab_util::GetWebContentsByID(process_id, routing_id);
  if (!web_contents)
    return;

  TabContents* tab = TabContents::FromWebContents(web_contents);
  if (!tab)
    return;

  const webkit::WebPluginInfo* reader = NULL;
  PluginFinder* plugin_finder = PluginFinder::GetInstance();
  for (size_t i = 0; i < plugins.size(); ++i) {
    PluginMetadata* plugin_metadata =
        plugin_finder->GetPluginMetadata(plugins[i]);
    if (plugin_metadata->identifier() != kAdobeReaderIdentifier)
      continue;

    DCHECK(!reader);
    reader = &plugins[i];
    // If the Reader plugin is disabled by policy, don't prompt them.
    PluginPrefs* plugin_prefs = PluginPrefs::GetForProfile(tab->profile());
    if (plugin_prefs->PolicyStatusForPlugin(plugin_metadata->name()) ==
        PluginPrefs::POLICY_DISABLED) {
      return;
    }
    break;
  }

  scoped_ptr<OpenPDFInReaderPromptDelegate> prompt(
      new PDFUnsupportedFeaturePromptDelegate(tab, reader, plugin_finder));
  PDFTabHelper* pdf_tab_helper = PDFTabHelper::FromWebContents(web_contents);
  pdf_tab_helper->ShowOpenInReaderPrompt(prompt.Pass());
}

}  // namespace

void PDFHasUnsupportedFeature(content::WebContents* web_contents) {
#if defined(OS_WIN) && defined(ENABLE_PLUGIN_INSTALLATION)
  // Only works for Windows for now.  For Mac, we'll have to launch the file
  // externally since Adobe Reader doesn't work inside Chrome.
  PluginService::GetInstance()->GetPlugins(base::Bind(&GotPluginsCallback,
      web_contents->GetRenderProcessHost()->GetID(),
      web_contents->GetRenderViewHost()->GetRoutingID()));
#endif
}
