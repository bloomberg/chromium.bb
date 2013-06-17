// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/nacl_ui.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/memory/weak_ptr.h"
#include "base/path_service.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/plugins/plugin_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "webkit/plugins/webplugininfo.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

using content::BrowserThread;
using content::PluginService;
using content::UserMetricsAction;
using content::WebUIMessageHandler;

namespace {

content::WebUIDataSource* CreateNaClUIHTMLSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUINaClHost);

  source->SetUseJsonJSFormatV2();
  source->AddLocalizedString("loadingMessage", IDS_NACL_LOADING_MESSAGE);
  source->AddLocalizedString("naclLongTitle", IDS_NACL_TITLE_MESSAGE);
  source->SetJsonPath("strings.js");
  source->AddResourcePath("about_nacl.css", IDR_ABOUT_NACL_CSS);
  source->AddResourcePath("about_nacl.js", IDR_ABOUT_NACL_JS);
  source->SetDefaultResource(IDR_ABOUT_NACL_HTML);
  return source;
}

////////////////////////////////////////////////////////////////////////////////
//
// NaClDomHandler
//
////////////////////////////////////////////////////////////////////////////////

class NaClDomHandler;

// This class performs a check that the PNaCl path which was returned by
// PathService is valid. One class instance is created per NaClDomHandler
// and it is destroyed after the check is completed.
class NaClDomHandlerProxy
    : public base::RefCountedThreadSafe<NaClDomHandlerProxy> {
 public:
  explicit NaClDomHandlerProxy(NaClDomHandler* handler);

  // A helper to check if PNaCl path exists.
  void ValidatePnaclPath();

  void set_handler(NaClDomHandler* handler) { handler_ = handler; }

 private:
  friend class base::RefCountedThreadSafe<NaClDomHandlerProxy>;
  virtual ~NaClDomHandlerProxy() {}

  // A helper callback that receives the result of checking if PNaCl path
  // exists.
  void ValidatePnaclPathCallback(bool is_valid);

  // The handler that requested checking PNaCl file path.
  NaClDomHandler* handler_;  // weak

  DISALLOW_COPY_AND_ASSIGN(NaClDomHandlerProxy);
};

// The handler for JavaScript messages for the about:flags page.
class NaClDomHandler : public WebUIMessageHandler {
 public:
  NaClDomHandler();
  virtual ~NaClDomHandler();

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

  // Callback for the "requestNaClInfo" message.
  void HandleRequestNaClInfo(const ListValue* args);

  // Callback for the NaCl plugin information.
  void OnGotPlugins(const std::vector<webkit::WebPluginInfo>& plugins);

  // A helper callback that receives the result of checking if PNaCl path
  // exists. |is_valid| is true if the PNaCl path that was returned by
  // PathService is valid, and false otherwise.
  void DidValidatePnaclPath(bool is_valid);

 private:
  // Called when enough information is gathered to return data back to the page.
  void MaybeRespondToPage();

  // Helper for MaybeRespondToPage -- called after enough information
  // is gathered.
  void PopulatePageInformation(DictionaryValue* naclInfo);

  // Factory for the creating refs in callbacks.
  base::WeakPtrFactory<NaClDomHandler> weak_ptr_factory_;

  // Whether the page has requested data.
  bool page_has_requested_data_;

  // Whether the plugin information is ready.
  bool has_plugin_info_;

  // Whether PNaCl path was validated. PathService can return a path
  // that does not exists, so it needs to be validated.
  bool pnacl_path_validated_;
  bool pnacl_path_exists_;

  // A proxy for handling cross threads messages.
  scoped_refptr<NaClDomHandlerProxy> proxy_;

  DISALLOW_COPY_AND_ASSIGN(NaClDomHandler);
};

NaClDomHandlerProxy::NaClDomHandlerProxy(NaClDomHandler* handler)
    : handler_(handler) {
}

void NaClDomHandlerProxy::ValidatePnaclPath() {
  if (!BrowserThread::CurrentlyOn(BrowserThread::FILE)) {
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        base::Bind(&NaClDomHandlerProxy::ValidatePnaclPath, this));
    return;
  }

  base::FilePath pnacl_path;
  bool got_path = PathService::Get(chrome::DIR_PNACL_COMPONENT, &pnacl_path);
  // The PathService may return an empty string if PNaCl is not yet installed.
  // However, do not trust that the path returned by the PathService exists.
  // Check for existence here.
  ValidatePnaclPathCallback(
    got_path && !pnacl_path.empty() && file_util::PathExists(pnacl_path));
}

void NaClDomHandlerProxy::ValidatePnaclPathCallback(bool is_valid) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&NaClDomHandlerProxy::ValidatePnaclPathCallback,
        this, is_valid));
    return;
  }

  // Check that handler_ is still valid, it could be set to NULL if navigation
  // happened while checking that the PNaCl file exists.
  if (handler_)
    handler_->DidValidatePnaclPath(is_valid);
}

NaClDomHandler::NaClDomHandler()
    : weak_ptr_factory_(this),
      page_has_requested_data_(false),
      has_plugin_info_(false),
      pnacl_path_validated_(false),
      pnacl_path_exists_(false),
      proxy_(new NaClDomHandlerProxy(this)) {
  PluginService::GetInstance()->GetPlugins(base::Bind(
      &NaClDomHandler::OnGotPlugins, weak_ptr_factory_.GetWeakPtr()));
}

NaClDomHandler::~NaClDomHandler() {
  if (proxy_.get())
    proxy_->set_handler(NULL);
}

void NaClDomHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "requestNaClInfo",
      base::Bind(&NaClDomHandler::HandleRequestNaClInfo,
                 base::Unretained(this)));
}

// Helper functions for collecting a list of key-value pairs that will
// be displayed.
void AddPair(ListValue* list, const string16& key, const string16& value) {
  DictionaryValue* results = new DictionaryValue();
  results->SetString("key", key);
  results->SetString("value", value);
  list->Append(results);
}

// Generate an empty data-pair which acts as a line break.
void AddLineBreak(ListValue* list) {
  AddPair(list, ASCIIToUTF16(""), ASCIIToUTF16(""));
}

// Check whether a commandline switch is turned on or off.
void ListFlagStatus(ListValue* list, const std::string& flag_label,
                    const std::string& flag_name) {
  if (CommandLine::ForCurrentProcess()->HasSwitch(flag_name))
    AddPair(list, ASCIIToUTF16(flag_label), ASCIIToUTF16("On"));
  else
    AddPair(list, ASCIIToUTF16(flag_label), ASCIIToUTF16("Off"));
}

void NaClDomHandler::HandleRequestNaClInfo(const ListValue* args) {
  page_has_requested_data_ = true;
  MaybeRespondToPage();
}

void NaClDomHandler::OnGotPlugins(
    const std::vector<webkit::WebPluginInfo>& plugins) {
  has_plugin_info_ = true;
  MaybeRespondToPage();
}

void NaClDomHandler::PopulatePageInformation(DictionaryValue* naclInfo) {
  DCHECK(pnacl_path_validated_);
  // Store Key-Value pairs of about-information.
  scoped_ptr<ListValue> list(new ListValue());

  // Obtain the Chrome version info.
  chrome::VersionInfo version_info;
  AddPair(list.get(),
          l10n_util::GetStringUTF16(IDS_PRODUCT_NAME),
          ASCIIToUTF16(version_info.Version() + " (" +
                       chrome::VersionInfo::GetVersionStringModifier() + ")"));

  // OS version information.
  // TODO(jvoung): refactor this to share the extra windows labeling
  // with about:flash, or something.
  std::string os_label = version_info.OSType();
#if defined(OS_WIN)
  base::win::OSInfo* os = base::win::OSInfo::GetInstance();
  switch (os->version()) {
    case base::win::VERSION_XP: os_label += " XP"; break;
    case base::win::VERSION_SERVER_2003:
      os_label += " Server 2003 or XP Pro 64 bit";
      break;
    case base::win::VERSION_VISTA: os_label += " Vista or Server 2008"; break;
    case base::win::VERSION_WIN7: os_label += " 7 or Server 2008 R2"; break;
    case base::win::VERSION_WIN8: os_label += " 8 or Server 2012"; break;
    default:  os_label += " UNKNOWN"; break;
  }
  os_label += " SP" + base::IntToString(os->service_pack().major);
  if (os->service_pack().minor > 0)
    os_label += "." + base::IntToString(os->service_pack().minor);
  if (os->architecture() == base::win::OSInfo::X64_ARCHITECTURE)
    os_label += " 64 bit";
#endif
  AddPair(list.get(),
          l10n_util::GetStringUTF16(IDS_ABOUT_VERSION_OS),
          ASCIIToUTF16(os_label));

  AddLineBreak(list.get());

  // Obtain the version of the NaCl plugin.
  std::vector<webkit::WebPluginInfo> info_array;
  PluginService::GetInstance()->GetPluginInfoArray(
      GURL(), "application/x-nacl", false, &info_array, NULL);
  string16 nacl_version;
  string16 nacl_key = ASCIIToUTF16("NaCl plugin");
  if (info_array.empty()) {
    AddPair(list.get(), nacl_key, ASCIIToUTF16("Disabled"));
  } else {
    PluginPrefs* plugin_prefs =
        PluginPrefs::GetForProfile(Profile::FromWebUI(web_ui())).get();

    // Only the 0th plugin is used.
    nacl_version = info_array[0].version + ASCIIToUTF16(" ") +
        info_array[0].path.LossyDisplayName();
    if (!plugin_prefs->IsPluginEnabled(info_array[0])) {
      nacl_version += ASCIIToUTF16(" (Disabled in profile prefs)");
      AddPair(list.get(), nacl_key, nacl_version);
    }

    AddPair(list.get(), nacl_key, nacl_version);

    // Mark the rest as not used.
    for (size_t i = 1; i < info_array.size(); ++i) {
      nacl_version = info_array[i].version + ASCIIToUTF16(" ") +
          info_array[i].path.LossyDisplayName();
      nacl_version += ASCIIToUTF16(" (not used)");
      if (!plugin_prefs->IsPluginEnabled(info_array[i]))
        nacl_version += ASCIIToUTF16(" (Disabled in profile prefs)");
      AddPair(list.get(), nacl_key, nacl_version);
    }
  }

  // Check that commandline flags are enabled.
  ListFlagStatus(list.get(), "Flag '--enable-nacl'", switches::kEnableNaCl);

  AddLineBreak(list.get());

  // Obtain the version of the PNaCl translator.
  base::FilePath pnacl_path;
  bool got_path = PathService::Get(chrome::DIR_PNACL_COMPONENT, &pnacl_path);
  if (!got_path || pnacl_path.empty() || !pnacl_path_exists_) {
    AddPair(list.get(),
            ASCIIToUTF16("PNaCl translator"),
            ASCIIToUTF16("Not installed"));
  } else {
    AddPair(list.get(),
            ASCIIToUTF16("PNaCl translator path"),
            pnacl_path.LossyDisplayName());
    // Version string is part of the directory name:
    // pnacl/<version>/_platform_specific/<arch>/[files]
    // Keep in sync with pnacl_component_installer.cc.
    AddPair(list.get(),
            ASCIIToUTF16("PNaCl translator version"),
            pnacl_path.DirName().DirName().BaseName().LossyDisplayName());
  }

  ListFlagStatus(list.get(), "Flag '--enable-pnacl'", switches::kEnablePnacl);
  // naclInfo will take ownership of list, and clean it up on destruction.
  naclInfo->Set("naclInfo", list.release());
}

void NaClDomHandler::DidValidatePnaclPath(bool is_valid) {
  pnacl_path_validated_ = true;
  pnacl_path_exists_ = is_valid;
  MaybeRespondToPage();
}

void NaClDomHandler::MaybeRespondToPage() {
  // Don't reply until everything is ready.  The page will show a 'loading'
  // message until then.
  if (!page_has_requested_data_ || !has_plugin_info_)
    return;

  if (!pnacl_path_validated_) {
    DCHECK(proxy_.get());
    proxy_->ValidatePnaclPath();
    return;
  }

  DictionaryValue naclInfo;
  PopulatePageInformation(&naclInfo);
  web_ui()->CallJavascriptFunction("nacl.returnNaClInfo", naclInfo);
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
//
// NaClUI
//
///////////////////////////////////////////////////////////////////////////////

NaClUI::NaClUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  content::RecordAction(UserMetricsAction("ViewAboutNaCl"));

  web_ui->AddMessageHandler(new NaClDomHandler());

  // Set up the about:nacl source.
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, CreateNaClUIHTMLSource());
}
