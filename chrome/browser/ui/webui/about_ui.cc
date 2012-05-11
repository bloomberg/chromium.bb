// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/about_ui.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/i18n/number_formatting.h"
#include "base/json/json_writer.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram.h"
#include "base/metrics/stats_table.h"
#include "base/path_service.h"
#include "base/string_number_conversions.h"
#include "base/string_piece.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/threading/thread.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/browser_about_handler.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/memory_details.h"
#include "chrome/browser/metrics/histogram_synchronizer.h"
#include "chrome/browser/net/predictor.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/browser/plugin_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/browser/ui/webui/chrome_web_ui_data_source.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/net/gaia/google_service_auth_error.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/common/content_client.h"
#include "content/public/common/process_type.h"
#include "crypto/nss_util.h"
#include "googleurl/src/gurl.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "net/base/escape.h"
#include "net/base/net_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "v8/include/v8.h"
#include "webkit/glue/user_agent.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/plugins/webplugininfo.h"

#if defined(OS_LINUX) || defined(OS_OPENBSD)
#include "content/public/common/sandbox_linux.h"
#include "content/public/browser/zygote_host_linux.h"
#endif

#if defined(OS_WIN)
#include "chrome/browser/enumerate_modules_model_win.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/cryptohome_library.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/customization_document.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/oom_priority_manager.h"
#include "chrome/browser/chromeos/version_loader.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#endif

#if defined(USE_ASH)
#include "ash/wm/frame_painter.h"
#include "base/string_split.h"
#endif

using base::Time;
using base::TimeDelta;
using content::BrowserThread;
using content::PluginService;
using content::WebContents;

namespace {

const char kCreditsJsPath[] = "credits.js";
const char kMemoryJsPath[] = "memory.js";
const char kStatsJsPath[] = "stats.js";
const char kStringsJsPath[] = "strings.js";
const char kVersionJsPath[] = "version.js";

// When you type about:memory, it actually loads this intermediate URL that
// redirects you to the final page. This avoids the problem where typing
// "about:memory" on the new tab page or any other page where a process
// transition would occur to the about URL will cause some confusion.
//
// The problem is that during the processing of the memory page, there are two
// processes active, the original and the destination one. This can create the
// impression that we're using more resources than we actually are. This
// redirect solves the problem by eliminating the process transition during the
// time that about memory is being computed.
std::string GetAboutMemoryRedirectResponse(Profile* profile) {
  return StringPrintf("<meta http-equiv=\"refresh\" content=\"0;%s\">",
                      chrome::kChromeUIMemoryRedirectURL);
}

// Handling about:memory is complicated enough to encapsulate its related
// methods into a single class. The user should create it (on the heap) and call
// its |StartFetch()| method.
class AboutMemoryHandler : public MemoryDetails {
 public:
  AboutMemoryHandler(AboutUIHTMLSource* source, int request_id)
      : source_(source),
        request_id_(request_id) {
  }

  virtual void OnDetailsAvailable() OVERRIDE;

 private:
  ~AboutMemoryHandler() {}

  void BindProcessMetrics(DictionaryValue* data,
                          ProcessMemoryInformation* info);
  void AppendProcess(ListValue* child_data, ProcessMemoryInformation* info);

  scoped_refptr<AboutUIHTMLSource> source_;
  int request_id_;

  DISALLOW_COPY_AND_ASSIGN(AboutMemoryHandler);
};

#if defined(OS_CHROMEOS)
// ChromeOSAboutVersionHandler is responsible for loading the Chrome OS
// version.
// ChromeOSAboutVersionHandler handles deleting itself once the version has
// been obtained and AboutUIHTMLSource notified.
class ChromeOSAboutVersionHandler {
 public:
  ChromeOSAboutVersionHandler(AboutUIHTMLSource* source, int request_id);

  // Callback from chromeos::VersionLoader giving the version.
  void OnVersion(chromeos::VersionLoader::Handle handle,
                 std::string version);

 private:
  // Where the results are fed to.
  scoped_refptr<AboutUIHTMLSource> source_;

  // ID identifying the request.
  int request_id_;

  // Handles asynchronously loading the version.
  chromeos::VersionLoader loader_;

  // Used to request the version.
  CancelableRequestConsumer consumer_;

  DISALLOW_COPY_AND_ASSIGN(ChromeOSAboutVersionHandler);
};

class ChromeOSTermsHandler
    : public base::RefCountedThreadSafe<ChromeOSTermsHandler> {
 public:
  static void Start(AboutUIHTMLSource* source,
                    const std::string& path,
                    int request_id) {
    scoped_refptr<ChromeOSTermsHandler> handler(
        new ChromeOSTermsHandler(source, path, request_id));
    handler->StartOnUIThread();
  }

 private:
  ChromeOSTermsHandler(AboutUIHTMLSource* source,
                       const std::string& path,
                       int request_id)
    : source_(source),
      path_(path),
      request_id_(request_id),
      locale_(chromeos::WizardController::GetInitialLocale()) {
  }

  void StartOnUIThread() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        base::Bind(&ChromeOSTermsHandler::LoadFileOnFileThread, this));
  }

  void LoadFileOnFileThread() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
    if (path_ == chrome::kOemEulaURLPath) {
      const chromeos::StartupCustomizationDocument* customization =
          chromeos::StartupCustomizationDocument::GetInstance();
      if (customization->IsReady()) {
        FilePath oem_eula_file_path;
        if (net::FileURLToFilePath(GURL(customization->GetEULAPage(locale_)),
                                   &oem_eula_file_path)) {
          if (!file_util::ReadFileToString(oem_eula_file_path, &contents_)) {
            contents_.clear();
          }
        }
      }
    } else {
      std::string file_path =
          StringPrintf(chrome::kEULAPathFormat, locale_.c_str());
      if (!file_util::ReadFileToString(FilePath(file_path), &contents_)) {
        // No EULA for given language - try en-US as default.
        file_path = StringPrintf(chrome::kEULAPathFormat, "en-US");
        if (!file_util::ReadFileToString(FilePath(file_path), &contents_)) {
          // File with EULA not found, ResponseOnUIThread will load EULA from
          // resources if contents_ is empty.
          contents_.clear();
        }
      }
    }
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&ChromeOSTermsHandler::ResponseOnUIThread, this));
  }

  void ResponseOnUIThread() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    // If we fail to load Chrome OS EULA from disk, load it from resources.
    // Do nothing if OEM EULA load failed.
    if (contents_.empty() && path_ != chrome::kOemEulaURLPath) {
      contents_ = ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_TERMS_HTML).as_string();
    }
    source_->FinishDataRequest(contents_, request_id_);
  }

  // Where the results are fed to.
  scoped_refptr<AboutUIHTMLSource> source_;

  // Path in the URL.
  std::string path_;

  // ID identifying the request.
  int request_id_;

  // Locale of the EULA.
  std::string locale_;

  // EULA contents that was loaded from file.
  std::string contents_;

  DISALLOW_COPY_AND_ASSIGN(ChromeOSTermsHandler);
};

#endif

// Individual about handlers ---------------------------------------------------

void AppendHeader(std::string* output, int refresh,
                  const std::string& unescaped_title) {
  output->append("<!DOCTYPE HTML>\n<html>\n<head>\n");
  if (!unescaped_title.empty()) {
    output->append("<title>");
    output->append(net::EscapeForHTML(unescaped_title));
    output->append("</title>\n");
  }
  output->append("<meta charset=\"utf-8\">\n");
  if (refresh > 0) {
    output->append("<meta http-equiv=\"refresh\" content=\"");
    output->append(base::IntToString(refresh));
    output->append("\"/>\n");
  }
}

void AppendBody(std::string *output) {
  output->append("</head>\n<body>\n");
}

void AppendFooter(std::string *output) {
  output->append("</body>\n</html>\n");
}

std::string ChromeURLs() {
  std::string html;
  AppendHeader(&html, 0, "Chrome URLs");
  AppendBody(&html);
  html += "<h2>List of Chrome URLs</h2>\n<ul>\n";
  std::vector<std::string> paths(ChromePaths());
  for (std::vector<std::string>::const_iterator i = paths.begin();
       i != paths.end(); ++i)
    html += "<li><a href='chrome://" + *i + "/'>chrome://" + *i + "</a></li>\n";
  html += "</ul>\n<h2>For Debug</h2>\n"
      "<p>The following pages are for debugging purposes only. Because they "
      "crash or hang the renderer, they're not linked directly; you can type "
      "them into the address bar if you need them.</p>\n<ul>";
  for (int i = 0; i < chrome::kNumberOfChromeDebugURLs; i++)
    html += "<li>" + std::string(chrome::kChromeDebugURLs[i]) + "</li>\n";
  html += "</ul>\n";
  AppendFooter(&html);
  return html;
}

#if defined(OS_CHROMEOS)

// Html output helper functions
// TODO(stevenjb): L10N this.

// Helper function to wrap HTML with a tag.
std::string WrapWithTag(const std::string& tag, const std::string& text) {
  return "<" + tag + ">" + text + "</" + tag + ">";
}

// Helper function to wrap Html with <th> tag.
std::string WrapWithTH(const std::string& text) {
  return "<th>" + text + "</th>";
}

// Helper function to wrap Html with <td> tag.
std::string WrapWithTD(const std::string& text) {
  return "<td>" + text + "</td>";
}

// Helper function to wrap Html with <tr> tag.
std::string WrapWithTR(const std::string& text) {
  return "<tr>" + text + "</tr>";
}

void AppendRefresh(std::string *output, int refresh, const std::string& name) {
  if (refresh > 0) {
    output->append("(Auto-refreshing page every ");
    output->append(base::IntToString(refresh));
    output->append("s)");
  } else {
    output->append("(To auto-refresh this page: about:");
    output->append(name);
    output->append("/&lt;secs&gt;)");
  }
}

// Helper function to create an Html table header for a Network.
std::string ToHtmlTableHeader(const chromeos::Network* network) {
  std::string str =
      WrapWithTH("Name") +
      WrapWithTH("Active") +
      WrapWithTH("State");
  if (network->type() == chromeos::TYPE_WIFI ||
      network->type() == chromeos::TYPE_CELLULAR) {
    str += WrapWithTH("Auto-Connect");
    str += WrapWithTH("Strength");
  }
  if (network->type() == chromeos::TYPE_WIFI) {
    str += WrapWithTH("Encryption");
    str += WrapWithTH("Passphrase");
    str += WrapWithTH("Identity");
  }
  if (network->type() == chromeos::TYPE_CELLULAR) {
    str += WrapWithTH("Technology");
    str += WrapWithTH("Connectivity");
    str += WrapWithTH("Activation");
    str += WrapWithTH("Roaming");
  }
  if (network->type() == chromeos::TYPE_VPN) {
    str += WrapWithTH("Host");
    str += WrapWithTH("Provider Type");
    str += WrapWithTH("PSK Passphrase");
    str += WrapWithTH("Username");
    str += WrapWithTH("User Passphrase");
  }
  str += WrapWithTH("Error");
  str += WrapWithTH("IP Address");
  return WrapWithTR(str);
}

// Helper function to create an Html table row for a Network.
std::string ToHtmlTableRow(const chromeos::Network* network) {
  std::string str =
      WrapWithTD(network->name()) +
      WrapWithTD(base::IntToString(network->is_active())) +
      WrapWithTD(network->GetStateString());
  if (network->type() == chromeos::TYPE_WIFI ||
      network->type() == chromeos::TYPE_CELLULAR) {
    const chromeos::WirelessNetwork* wireless =
        static_cast<const chromeos::WirelessNetwork*>(network);
    str += WrapWithTD(base::IntToString(wireless->auto_connect()));
    str += WrapWithTD(base::IntToString(wireless->strength()));
  }
  if (network->type() == chromeos::TYPE_WIFI) {
    const chromeos::WifiNetwork* wifi =
        static_cast<const chromeos::WifiNetwork*>(network);
    str += WrapWithTD(wifi->GetEncryptionString());
    str += WrapWithTD(std::string(wifi->passphrase().length(), '*'));
    str += WrapWithTD(wifi->identity());
  }
  if (network->type() == chromeos::TYPE_CELLULAR) {
    const chromeos::CellularNetwork* cell =
        static_cast<const chromeos::CellularNetwork*>(network);
    str += WrapWithTH(cell->GetNetworkTechnologyString());
    str += WrapWithTH(cell->GetActivationStateString());
    str += WrapWithTH(cell->GetRoamingStateString());
  }
  if (network->type() == chromeos::TYPE_VPN) {
    const chromeos::VirtualNetwork* vpn =
        static_cast<const chromeos::VirtualNetwork*>(network);
    str += WrapWithTH(vpn->server_hostname());
    str += WrapWithTH(vpn->GetProviderTypeString());
    str += WrapWithTD(std::string(vpn->psk_passphrase().length(), '*'));
    str += WrapWithTH(vpn->username());
    str += WrapWithTD(std::string(vpn->user_passphrase().length(), '*'));
  }
  str += WrapWithTD(network->failed() ? network->GetErrorString() : "");
  str += WrapWithTD(network->ip_address());
  return WrapWithTR(str);
}

std::string GetNetworkHtmlInfo(int refresh) {
  chromeos::NetworkLibrary* cros =
      chromeos::CrosLibrary::Get()->GetNetworkLibrary();
  std::string output;
  AppendHeader(&output, refresh, "About Network");
  AppendBody(&output);
  AppendRefresh(&output, refresh, "network");

  if (cros->ethernet_enabled()) {
    output.append("<h3>Ethernet:</h3><table border=1>");
    const chromeos::EthernetNetwork* ethernet = cros->ethernet_network();
    if (ethernet) {
      output.append(ToHtmlTableHeader(ethernet));
      output.append(ToHtmlTableRow(ethernet));
    }
  }

  if (cros->wifi_enabled()) {
    output.append("</table><h3>Wifi Networks:</h3><table border=1>");
    const chromeos::WifiNetworkVector& wifi_networks = cros->wifi_networks();
    for (size_t i = 0; i < wifi_networks.size(); ++i) {
      if (i == 0)
        output.append(ToHtmlTableHeader(wifi_networks[i]));
      output.append(ToHtmlTableRow(wifi_networks[i]));
    }
  }

  if (cros->cellular_enabled()) {
    output.append("</table><h3>Cellular Networks:</h3><table border=1>");
    const chromeos::CellularNetworkVector& cellular_networks =
        cros->cellular_networks();
    for (size_t i = 0; i < cellular_networks.size(); ++i) {
      if (i == 0)
        output.append(ToHtmlTableHeader(cellular_networks[i]));
      output.append(ToHtmlTableRow(cellular_networks[i]));
    }
  }

  {
    output.append("</table><h3>Virtual Networks:</h3><table border=1>");
    const chromeos::VirtualNetworkVector& virtual_networks =
        cros->virtual_networks();
    for (size_t i = 0; i < virtual_networks.size(); ++i) {
      if (i == 0)
        output.append(ToHtmlTableHeader(virtual_networks[i]));
      output.append(ToHtmlTableRow(virtual_networks[i]));
    }
  }

  {
    output.append(
        "</table><h3>Remembered Wi-Fi Networks:</h3><table border=1>");
    const chromeos::WifiNetworkVector& remembered_wifi_networks =
        cros->remembered_wifi_networks();
    for (size_t i = 0; i < remembered_wifi_networks.size(); ++i) {
      if (i == 0)
        output.append(
            ToHtmlTableHeader(remembered_wifi_networks[i]));
      output.append(ToHtmlTableRow(remembered_wifi_networks[i]));
    }
  }

  output.append("</table>");
  AppendFooter(&output);
  return output;
}

std::string AboutNetwork(const std::string& query) {
  int refresh;
  base::StringToInt(query, &refresh);
  return GetNetworkHtmlInfo(refresh);
}

std::string AddBoolRow(const std::string& name, bool value) {
  std::string row;
  row.append(WrapWithTD(name));
  row.append(WrapWithTD(value ? "true" : "false"));
  return WrapWithTR(row);
}

std::string AddStringRow(const std::string& name, const std::string& value) {
  std::string row;
  row.append(WrapWithTD(name));
  row.append(WrapWithTD(value));
  return WrapWithTR(row);
}

void FinishCryptohomeDataRequestInternal(
    scoped_refptr<AboutUIHTMLSource> source,
    int refresh,
    int request_id,
    chromeos::DBusMethodCallStatus call_status,
    bool is_tpm_token_ready) {
  if (call_status != chromeos::DBUS_METHOD_CALL_SUCCESS)
    is_tpm_token_ready = false;

  chromeos::CryptohomeLibrary* cryptohome =
      chromeos::CrosLibrary::Get()->GetCryptohomeLibrary();
  std::string output;
  AppendHeader(&output, refresh, "About Cryptohome");
  AppendBody(&output);
  AppendRefresh(&output, refresh, "cryptohome");

  output.append("<h3>CryptohomeLibrary:</h3>");
  output.append("<table>");
  output.append(AddBoolRow("IsMounted", cryptohome->IsMounted()));
  output.append(AddBoolRow("TpmIsReady", cryptohome->TpmIsReady()));
  output.append(AddBoolRow("TpmIsEnabled", cryptohome->TpmIsEnabled()));
  output.append(AddBoolRow("TpmIsOwned", cryptohome->TpmIsOwned()));
  output.append(AddBoolRow("TpmIsBeingOwned", cryptohome->TpmIsBeingOwned()));
  output.append(AddBoolRow("Pkcs11IsTpmTokenReady", is_tpm_token_ready));
  output.append("</table>");

  output.append("<h3>crypto:</h3>");
  output.append("<table>");
  output.append(AddBoolRow("IsTPMTokenReady", crypto::IsTPMTokenReady()));
  std::string token_name, user_pin;
  if (crypto::IsTPMTokenReady())
    crypto::GetTPMTokenInfo(&token_name, &user_pin);
  output.append(AddStringRow("token_name", token_name));
  output.append(AddStringRow("user_pin", std::string(user_pin.length(), '*')));
  output.append("</table>");
  AppendFooter(&output);

  source->FinishDataRequest(output, request_id);
}

void FinishCryptohomeDataRequest(scoped_refptr<AboutUIHTMLSource> source,
                                 const std::string& query,
                                 int request_id) {
  int refresh;
  base::StringToInt(query, &refresh);

  chromeos::DBusThreadManager::Get()->GetCryptohomeClient()->
      Pkcs11IsTpmTokenReady(base::Bind(&FinishCryptohomeDataRequestInternal,
                                       source,
                                       refresh,
                                       request_id));
}

std::string AboutDiscardsRun() {
  std::string output;
  AppendHeader(&output, 0, "About discards");
  output.append(StringPrintf("<meta http-equiv=\"refresh\" content=\"2;%s\">",
                             chrome::kChromeUIDiscardsURL));
  output.append(WrapWithTag("p", "Discarding a tab..."));
  g_browser_process->oom_priority_manager()->LogMemoryAndDiscardTab();
  AppendFooter(&output);
  return output;
}

std::string AboutDiscards(const std::string& path) {
  std::string output;
  const char kRunCommand[] = "run";
  if (path == kRunCommand)
    return AboutDiscardsRun();
  AppendHeader(&output, 0, "About discards");
  AppendBody(&output);
  output.append("<h3>About discards</h3>");
  output.append(
      "<p>Tabs sorted from most interesting to least interesting. The least "
      "interesting tab may be discarded if we run out of physical memory.</p>");

  chromeos::OomPriorityManager* oom = g_browser_process->oom_priority_manager();
  std::vector<string16> titles = oom->GetTabTitles();
  if (!titles.empty()) {
    output.append("<ol>");
    std::vector<string16>::iterator it = titles.begin();
    for ( ; it != titles.end(); ++it) {
      std::string title = UTF16ToUTF8(*it);
      output.append(WrapWithTag("li", title));
    }
    output.append("</ol>");
  } else {
    output.append("<p>None found.  Wait 10 seconds, then refresh.</p>");
  }
  output.append(StringPrintf("%d discards this session. ",
                             oom->discard_count()));
  output.append(StringPrintf("<a href='%s%s'>Discard tab now</a>",
                             chrome::kChromeUIDiscardsURL,
                             kRunCommand));

  base::SystemMemoryInfoKB meminfo;
  base::GetSystemMemoryInfo(&meminfo);
  output.append("<h3>System memory information in MB</h3>");
  output.append("<table>");
  output.append(AddStringRow(
    "Total", base::IntToString(meminfo.total / 1024)));
  output.append(AddStringRow(
    "Free", base::IntToString(meminfo.free / 1024)));
  output.append(AddStringRow(
    "Buffered", base::IntToString(meminfo.buffers / 1024)));
  output.append(AddStringRow(
    "Cached", base::IntToString(meminfo.cached / 1024)));
  output.append(AddStringRow(
    "Committed", base::IntToString(
    (meminfo.total - meminfo.free - meminfo.buffers - meminfo.cached) / 1024)));
  output.append(AddStringRow(
    "Active Anon", base::IntToString(meminfo.active_anon / 1024)));
  output.append(AddStringRow(
    "Inactive Anon", base::IntToString(meminfo.inactive_anon / 1024)));
  output.append(AddStringRow(
    "Shared", base::IntToString(meminfo.shmem / 1024)));
  output.append("</table>");

  AppendFooter(&output);
  return output;
}

#endif  // OS_CHROMEOS

#if defined(USE_ASH)

// Adds an entry to the chrome://transparency page, with the format:
// |label|:
// -- - |value|% + ++
// where --, -, +, and ++ are links to change the appropriate |key|.
// TODO(jamescook): Remove this temporary tool when we decide what the window
// header opacity should be for Ash.
std::string TransparencyLink(const std::string& label,
                             int value,
                             const std::string& key) {
  return StringPrintf("<p>%s</p>"
      "<p>"
      "<a href='%s%s=%d'>--</a> "
      "<a href='%s%s=%d'>-</a> "
      "%d%% "
      "<a href='%s%s=%d'>+</a> "
      "<a href='%s%s=%d'>++</a>"
      "</p>",
      label.c_str(),
      chrome::kChromeUITransparencyURL, key.c_str(), value - 5,
      chrome::kChromeUITransparencyURL, key.c_str(), value - 1,
      value,
      chrome::kChromeUITransparencyURL, key.c_str(), value + 1,
      chrome::kChromeUITransparencyURL, key.c_str(), value + 5);
}

// Returns a transparency percent from an opacity value.
int TransparencyFromOpacity(int opacity) {
  return (255 - opacity) * 100 / 255;
}

// Displays a tweaking page for window header transparency, as we're iterating
// rapidly on how transparent we want the headers to be.
// TODO(jamescook): Remove this temporary tool when we decide what the window
// header opacity should be for Ash.
std::string AboutTransparency(const std::string& path) {
  const char kActive[] = "active";
  const char kInactive[] = "inactive";
  const char kSolo[] = "solo";
  // Apply transparency based on a path like "active=10&inactive=25".
  std::vector<std::pair<std::string, std::string> > kv_pairs;
  if (!path.empty() &&
      base::SplitStringIntoKeyValuePairs(path, '=', '&', &kv_pairs)) {
    for (std::vector<std::pair<std::string, std::string> >::const_iterator it =
            kv_pairs.begin();
         it != kv_pairs.end();
         ++it) {
      int* opacity = NULL;
      if (it->first == kActive)
        opacity = &ash::FramePainter::kActiveWindowOpacity;
      else if (it->first == kInactive)
        opacity = &ash::FramePainter::kInactiveWindowOpacity;
      else if (it->first == kSolo)
        opacity = &ash::FramePainter::kSoloWindowOpacity;
      if (opacity) {
        int transparent = 0;
        base::StringToInt(it->second, &transparent);
        transparent = std::max(0, std::min(100, transparent));
        *opacity = (100 - transparent) * 255 / 100;
      }
    }
  }

  // Display current settings.  Do everything in transparency-percent instead of
  // opacity-value.
  std::string output;
  AppendHeader(&output, 0, "About transparency");
  AppendFooter(&output);
  AppendBody(&output);
  output.append("<h3>Window header transparency</h3>");
  output.append(TransparencyLink("Active window:",
      TransparencyFromOpacity(ash::FramePainter::kActiveWindowOpacity),
      kActive));
  output.append(TransparencyLink("Inactive window:",
      TransparencyFromOpacity(ash::FramePainter::kInactiveWindowOpacity),
      kInactive));
  output.append(TransparencyLink("Solo window:",
      TransparencyFromOpacity(ash::FramePainter::kSoloWindowOpacity),
      kSolo));
  output.append(StringPrintf(
      "<p>Share: %s%s=%d&%s=%d&%s=%d</p>",
      chrome::kChromeUITransparencyURL,
      kActive,
      TransparencyFromOpacity(ash::FramePainter::kActiveWindowOpacity),
      kInactive,
      TransparencyFromOpacity(ash::FramePainter::kInactiveWindowOpacity),
      kSolo,
      TransparencyFromOpacity(ash::FramePainter::kSoloWindowOpacity)));
  output.append("<p>Reshape window to force a redraw.</p>");
  AppendFooter(&output);
  return output;
}

#endif  // USE_ASH

// AboutDnsHandler bounces the request back to the IO thread to collect
// the DNS information.
class AboutDnsHandler : public base::RefCountedThreadSafe<AboutDnsHandler> {
 public:
  static void Start(AboutUIHTMLSource* source, int request_id) {
    scoped_refptr<AboutDnsHandler> handler(
        new AboutDnsHandler(source, request_id));
    handler->StartOnUIThread();
  }

 private:
  friend class base::RefCountedThreadSafe<AboutDnsHandler>;

  AboutDnsHandler(AboutUIHTMLSource* source, int request_id)
      : source_(source),
        request_id_(request_id) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  }

  virtual ~AboutDnsHandler() {}

  // Calls FinishOnUIThread() on completion.
  void StartOnUIThread() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    chrome_browser_net::Predictor* predictor =
        source_->profile()->GetNetworkPredictor();
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&AboutDnsHandler::StartOnIOThread, this, predictor));
  }

  void StartOnIOThread(chrome_browser_net::Predictor* predictor) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

    std::string data;
    AppendHeader(&data, 0, "About DNS");
    AppendBody(&data);
    chrome_browser_net::Predictor::PredictorGetHtmlInfo(predictor, &data);
    AppendFooter(&data);

    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&AboutDnsHandler::FinishOnUIThread, this, data));
  }

  void FinishOnUIThread(const std::string& data) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    source_->FinishDataRequest(data, request_id_);
  }

  // Where the results are fed to.
  scoped_refptr<AboutUIHTMLSource> source_;

  // ID identifying the request.
  int request_id_;

  DISALLOW_COPY_AND_ASSIGN(AboutDnsHandler);
};

std::string AboutHistograms(const std::string& query) {
  TimeDelta wait_time = TimeDelta::FromMilliseconds(10000);

#ifndef NDEBUG
  base::StatisticsRecorder::CollectHistogramStats("Browser");
#endif

  HistogramSynchronizer* current_synchronizer =
      HistogramSynchronizer::CurrentSynchronizer();
  DCHECK(current_synchronizer != NULL);
  current_synchronizer->FetchRendererHistogramsSynchronously(wait_time);

  std::string unescaped_query;
  std::string unescaped_title("About Histograms");
  if (!query.empty()) {
    unescaped_query = net::UnescapeURLComponent(query,
                                                net::UnescapeRule::NORMAL);
    unescaped_title += " - " + unescaped_query;
  }

  std::string data;
  AppendHeader(&data, 0, unescaped_title);
  AppendBody(&data);
  base::StatisticsRecorder::WriteHTMLGraph(unescaped_query, &data);
  AppendFooter(&data);
  return data;
}

void FinishMemoryDataRequest(const std::string& path,
                             AboutUIHTMLSource* source,
                             int request_id) {
  if (path == kStringsJsPath) {
    // The AboutMemoryHandler cleans itself up, but |StartFetch()| will want
    // the refcount to be greater than 0.
    scoped_refptr<AboutMemoryHandler>
        handler(new AboutMemoryHandler(source, request_id));
    // TODO(jamescook): Maybe this shouldn't update UMA?
    handler->StartFetch(MemoryDetails::UPDATE_USER_METRICS);
  } else {
    source->FinishDataRequest(
        ResourceBundle::GetSharedInstance().GetRawDataResource(
            path == kMemoryJsPath ? IDR_ABOUT_MEMORY_JS :
                IDR_ABOUT_MEMORY_HTML).as_string(), request_id);
  }
}

// Handler for filling in the "about:stats" page, as called by the browser's
// About handler processing.
// |query| is roughly the query string of the about:stats URL.
// Returns a string containing the HTML to render for the about:stats page.
// Conditional Output:
//      if |query| is "json", returns a JSON format of all counters.
//      if |query| is "raw", returns plain text of counter deltas.
//      otherwise, returns HTML with pretty JS/HTML to display the data.
std::string AboutStats(const std::string& query) {
  // We keep the DictionaryValue tree live so that we can do delta
  // stats computations across runs.
  CR_DEFINE_STATIC_LOCAL(DictionaryValue, root, ());
  static base::TimeTicks last_sample_time = base::TimeTicks::Now();

  base::TimeTicks now = base::TimeTicks::Now();
  base::TimeDelta time_since_last_sample = now - last_sample_time;
  last_sample_time = now;

  base::StatsTable* table = base::StatsTable::current();
  if (!table)
    return std::string();

  // We maintain two lists - one for counters and one for timers.
  // Timers actually get stored on both lists.
  ListValue* counters;
  if (!root.GetList("counters", &counters)) {
    counters = new ListValue();
    root.Set("counters", counters);
  }

  ListValue* timers;
  if (!root.GetList("timers", &timers)) {
    timers = new ListValue();
    root.Set("timers", timers);
  }

  // NOTE: Counters start at index 1.
  for (int index = 1; index <= table->GetMaxCounters(); index++) {
    // Get the counter's full name
    std::string full_name = table->GetRowName(index);
    if (full_name.length() == 0)
      break;
    DCHECK_EQ(':', full_name[1]);
    char counter_type = full_name[0];
    std::string name = full_name.substr(2);

    // JSON doesn't allow '.' in names.
    size_t pos;
    while ((pos = name.find(".")) != std::string::npos)
      name.replace(pos, 1, ":");

    // Try to see if this name already exists.
    DictionaryValue* counter = NULL;
    for (size_t scan_index = 0;
         scan_index < counters->GetSize(); scan_index++) {
      DictionaryValue* dictionary;
      if (counters->GetDictionary(scan_index, &dictionary)) {
        std::string scan_name;
        if (dictionary->GetString("name", &scan_name) && scan_name == name) {
          counter = dictionary;
        }
      } else {
        NOTREACHED();  // Should always be there
      }
    }

    if (counter == NULL) {
      counter = new DictionaryValue();
      counter->SetString("name", name);
      counters->Append(counter);
    }

    switch (counter_type) {
      case 'c':
        {
          int new_value = table->GetRowValue(index);
          int prior_value = 0;
          int delta = 0;
          if (counter->GetInteger("value", &prior_value)) {
            delta = new_value - prior_value;
          }
          counter->SetInteger("value", new_value);
          counter->SetInteger("delta", delta);
        }
        break;
      case 'm':
        {
          // TODO(mbelshe): implement me.
        }
        break;
      case 't':
        {
          int time = table->GetRowValue(index);
          counter->SetInteger("time", time);

          // Store this on the timers list as well.
          timers->Append(counter);
        }
        break;
      default:
        NOTREACHED();
    }
  }

  std::string data;
  if (query == "json" || query == kStringsJsPath) {
    base::JSONWriter::WriteWithOptions(
          &root,
          base::JSONWriter::OPTIONS_DO_NOT_ESCAPE |
              base::JSONWriter::OPTIONS_PRETTY_PRINT,
          &data);
    if (query == kStringsJsPath)
      data = "var templateData = " + data + ";";
  } else if (query == "raw") {
    // Dump the raw counters which have changed in text format.
    data = "<pre>";
    data.append(StringPrintf("Counter changes in the last %ldms\n",
        static_cast<long int>(time_since_last_sample.InMilliseconds())));
    for (size_t i = 0; i < counters->GetSize(); ++i) {
      Value* entry = NULL;
      bool rv = counters->Get(i, &entry);
      if (!rv)
        continue;  // None of these should fail.
      DictionaryValue* counter = static_cast<DictionaryValue*>(entry);
      int delta;
      rv = counter->GetInteger("delta", &delta);
      if (!rv)
        continue;
      if (delta > 0) {
        std::string name;
        rv = counter->GetString("name", &name);
        if (!rv)
          continue;
        int value;
        rv = counter->GetInteger("value", &value);
        if (!rv)
          continue;
        data.append(name);
        data.append(":");
        data.append(base::IntToString(delta));
        data.append("\n");
      }
    }
    data.append("</pre>");
  } else {
    // Get about_stats.html/js from resource bundle.
    data = ResourceBundle::GetSharedInstance().GetRawDataResource(
        (query == kStatsJsPath ? IDR_ABOUT_STATS_JS : IDR_ABOUT_STATS_HTML)).
        as_string();

    if (query != kStatsJsPath) {
      // Clear the timer list since we stored the data in the timers list
      // as well.
      for (int index = static_cast<int>(timers->GetSize())-1; index >= 0;
           index--) {
        Value* value;
        timers->Remove(index, &value);
        // We don't care about the value pointer; it's still tracked
        // on the counters list.
      }
    }
  }

  return data;
}

#if defined(OS_LINUX) || defined(OS_OPENBSD)
std::string AboutLinuxProxyConfig() {
  std::string data;
  AppendHeader(&data, 0,
               l10n_util::GetStringUTF8(IDS_ABOUT_LINUX_PROXY_CONFIG_TITLE));
  data.append("<style>body { max-width: 70ex; padding: 2ex 5ex; }</style>");
  AppendBody(&data);
  FilePath binary = CommandLine::ForCurrentProcess()->GetProgram();
  data.append(l10n_util::GetStringFUTF8(
      IDS_ABOUT_LINUX_PROXY_CONFIG_BODY,
      l10n_util::GetStringUTF16(IDS_PRODUCT_NAME),
      ASCIIToUTF16(binary.BaseName().value())));
  AppendFooter(&data);
  return data;
}

void AboutSandboxRow(std::string* data, const std::string& prefix, int name_id,
                     bool good) {
  data->append("<tr><td>");
  data->append(prefix);
  data->append(l10n_util::GetStringUTF8(name_id));
  if (good) {
    data->append("</td><td style=\"color: green;\">");
    data->append(
        l10n_util::GetStringUTF8(IDS_CONFIRM_MESSAGEBOX_YES_BUTTON_LABEL));
  } else {
    data->append("</td><td style=\"color: red;\">");
    data->append(
        l10n_util::GetStringUTF8(IDS_CONFIRM_MESSAGEBOX_NO_BUTTON_LABEL));
  }
  data->append("</td></tr>");
}

std::string AboutSandbox() {
  std::string data;
  AppendHeader(&data, 0, l10n_util::GetStringUTF8(IDS_ABOUT_SANDBOX_TITLE));
  AppendBody(&data);
  data.append("<h1>");
  data.append(l10n_util::GetStringUTF8(IDS_ABOUT_SANDBOX_TITLE));
  data.append("</h1>");

  const int status = content::ZygoteHost::GetInstance()->GetSandboxStatus();

  data.append("<table>");

  AboutSandboxRow(&data, "", IDS_ABOUT_SANDBOX_SUID_SANDBOX,
                  status & content::kSandboxLinuxSUID);
  AboutSandboxRow(&data, "&nbsp;&nbsp;", IDS_ABOUT_SANDBOX_PID_NAMESPACES,
                  status & content::kSandboxLinuxPIDNS);
  AboutSandboxRow(&data, "&nbsp;&nbsp;", IDS_ABOUT_SANDBOX_NET_NAMESPACES,
                  status & content::kSandboxLinuxNetNS);
  AboutSandboxRow(&data, "", IDS_ABOUT_SANDBOX_SECCOMP_SANDBOX,
                  status & content::kSandboxLinuxSeccomp);

  data.append("</table>");

  bool good = ((status & content::kSandboxLinuxSUID) &&
               (status & content::kSandboxLinuxPIDNS)) ||
              (status & content::kSandboxLinuxSeccomp);
  if (good) {
    data.append("<p style=\"color: green\">");
    data.append(l10n_util::GetStringUTF8(IDS_ABOUT_SANDBOX_OK));
  } else {
    data.append("<p style=\"color: red\">");
    data.append(l10n_util::GetStringUTF8(IDS_ABOUT_SANDBOX_BAD));
  }
  data.append("</p>");

  AppendFooter(&data);
  return data;
}
#endif

std::string AboutVersionStaticContent(const std::string& query) {
  return ResourceBundle::GetSharedInstance().GetRawDataResource(
      query ==  kVersionJsPath ?
      IDR_ABOUT_VERSION_JS :
      IDR_ABOUT_VERSION_HTML).as_string();
}

std::string AboutVersionStrings(DictionaryValue* localized_strings,
                                Profile* profile) {
  DCHECK(profile);
  localized_strings->SetString("title",
      l10n_util::GetStringUTF16(IDS_ABOUT_VERSION_TITLE));
  chrome::VersionInfo version_info;

  localized_strings->SetString("name",
      l10n_util::GetStringUTF16(IDS_PRODUCT_NAME));
  localized_strings->SetString("version", version_info.Version());
  // Bug 79458: Need to evaluate the use of getting the version string on
  // this thread.
  base::ThreadRestrictions::ScopedAllowIO allow_io;
  localized_strings->SetString("version_modifier",
                               chrome::VersionInfo::GetVersionStringModifier());
  localized_strings->SetString("os_name",
                               l10n_util::GetStringUTF16(IDS_ABOUT_VERSION_OS));
  localized_strings->SetString("platform",
                               l10n_util::GetStringUTF16(IDS_PLATFORM_LABEL));
  localized_strings->SetString("os_type", version_info.OSType());
  localized_strings->SetString("webkit_version",
                               webkit_glue::GetWebKitVersion());
  localized_strings->SetString("js_engine", "V8");
  localized_strings->SetString("js_version", v8::V8::GetVersion());

  // Obtain the version of the first enabled Flash plugin.
  std::vector<webkit::WebPluginInfo> info_array;
  PluginService::GetInstance()->GetPluginInfoArray(
      GURL(), "application/x-shockwave-flash", false, &info_array, NULL);
  string16 flash_version =
      l10n_util::GetStringUTF16(IDS_PLUGINS_DISABLED_PLUGIN);
  PluginPrefs* plugin_prefs = PluginPrefs::GetForProfile(profile);
  if (plugin_prefs) {
    for (size_t i = 0; i < info_array.size(); ++i) {
      if (plugin_prefs->IsPluginEnabled(info_array[i])) {
        flash_version = info_array[i].version;
        break;
      }
    }
  }
  localized_strings->SetString("flash_plugin", "Flash");
  localized_strings->SetString("flash_version", flash_version);
  localized_strings->SetString("company",
      l10n_util::GetStringUTF16(IDS_ABOUT_VERSION_COMPANY_NAME));
  localized_strings->SetString("copyright",
      l10n_util::GetStringUTF16(IDS_ABOUT_VERSION_COPYRIGHT));
  localized_strings->SetString("cl", version_info.LastChange());
  localized_strings->SetString("official",
      l10n_util::GetStringUTF16(
          version_info.IsOfficialBuild() ?
              IDS_ABOUT_VERSION_OFFICIAL
            : IDS_ABOUT_VERSION_UNOFFICIAL));
  localized_strings->SetString("user_agent_name",
      l10n_util::GetStringUTF16(IDS_ABOUT_VERSION_USER_AGENT));
  localized_strings->SetString("useragent", content::GetUserAgent(GURL()));
  localized_strings->SetString("command_line_name",
      l10n_util::GetStringUTF16(IDS_ABOUT_VERSION_COMMAND_LINE));

#if defined(OS_WIN)
  localized_strings->SetString("command_line",
      WideToUTF16(CommandLine::ForCurrentProcess()->GetCommandLineString()));
#elif defined(OS_POSIX)
  std::string command_line = "";
  typedef std::vector<std::string> ArgvList;
  const ArgvList& argv = CommandLine::ForCurrentProcess()->argv();
  for (ArgvList::const_iterator iter = argv.begin(); iter != argv.end(); iter++)
    command_line += " " + *iter;
  // TODO(viettrungluu): |command_line| could really have any encoding, whereas
  // below we assumes it's UTF-8.
  localized_strings->SetString("command_line", command_line);
#endif

  // Allow IO temporarily based on allow_io (defined above)
  // since the following operation will complete quickly
  localized_strings->SetString("executable_path_name",
      l10n_util::GetStringUTF16(IDS_ABOUT_VERSION_EXECUTABLE_PATH));
  FilePath executable_path = CommandLine::ForCurrentProcess()->GetProgram();
  if (file_util::AbsolutePath(&executable_path)) {
    localized_strings->SetString("executable_path", executable_path.value());
  } else {
    localized_strings->SetString("executable_path",
        l10n_util::GetStringUTF16(IDS_ABOUT_VERSION_PATH_NOTFOUND));
  }
  localized_strings->SetString("profile_path_name",
      l10n_util::GetStringUTF16(IDS_ABOUT_VERSION_PROFILE_PATH));
  if (profile) {
    FilePath profile_path = profile->GetPath();
    if (file_util::AbsolutePath(&profile_path)) {
      localized_strings->SetString("profile_path", profile_path.value());
    } else {
      localized_strings->SetString("profile_path",
          l10n_util::GetStringUTF16(IDS_ABOUT_VERSION_PATH_NOTFOUND));
    }
  } else {
    localized_strings->SetString("profile_path",
        l10n_util::GetStringUTF16(IDS_ABOUT_VERSION_PATH_NOTFOUND));
  }
  ChromeWebUIDataSource::SetFontAndTextDirection(localized_strings);

  std::string data;
  jstemplate_builder::AppendJsonJS(localized_strings, &data);
  return data;
}

// Used as a callback for PluginService::GetPlugins().
void HandleAboutVersionStrings(AboutUIHTMLSource* source,
                               int request_id,
                               const std::vector<webkit::WebPluginInfo>&) {
#if defined(OS_CHROMEOS)
  new ChromeOSAboutVersionHandler(source, request_id);
#else
  DictionaryValue localized_strings;
  localized_strings.SetString("os_version", "");
  source->FinishDataRequest(
      AboutVersionStrings(&localized_strings, source->profile()),
      request_id);
#endif
}

// AboutMemoryHandler ----------------------------------------------------------

// Helper for AboutMemory to bind results from a ProcessMetrics object
// to a DictionaryValue. Fills ws_usage and comm_usage so that the objects
// can be used in caller's scope (e.g for appending to a net total).
void AboutMemoryHandler::BindProcessMetrics(DictionaryValue* data,
                                            ProcessMemoryInformation* info) {
  DCHECK(data && info);

  // Bind metrics to dictionary.
  data->SetInteger("ws_priv", static_cast<int>(info->working_set.priv));
  data->SetInteger("ws_shareable",
    static_cast<int>(info->working_set.shareable));
  data->SetInteger("ws_shared", static_cast<int>(info->working_set.shared));
  data->SetInteger("comm_priv", static_cast<int>(info->committed.priv));
  data->SetInteger("comm_map", static_cast<int>(info->committed.mapped));
  data->SetInteger("comm_image", static_cast<int>(info->committed.image));
  data->SetInteger("pid", info->pid);
  data->SetString("version", info->version);
  data->SetInteger("processes", info->num_processes);
}

// Helper for AboutMemory to append memory usage information for all
// sub-processes (i.e. renderers, plugins) used by Chrome.
void AboutMemoryHandler::AppendProcess(ListValue* child_data,
                                       ProcessMemoryInformation* info) {
  DCHECK(child_data && info);

  // Append a new DictionaryValue for this renderer to our list.
  DictionaryValue* child = new DictionaryValue();
  child_data->Append(child);
  BindProcessMetrics(child, info);

  std::string child_label(
      ProcessMemoryInformation::GetFullTypeNameInEnglish(info->type,
                                                         info->renderer_type));
  if (info->is_diagnostics)
    child_label.append(" (diagnostics)");
  child->SetString("child_name", child_label);
  ListValue* titles = new ListValue();
  child->Set("titles", titles);
  for (size_t i = 0; i < info->titles.size(); ++i)
    titles->Append(new StringValue(info->titles[i]));
}


void AboutMemoryHandler::OnDetailsAvailable() {
  // the root of the JSON hierarchy for about:memory jstemplate
  DictionaryValue root;
  ListValue* browsers = new ListValue();
  root.Set("browsers", browsers);

  const std::vector<ProcessData>& browser_processes = processes();

  // Aggregate per-process data into browser summary data.
  std::wstring log_string;
  for (size_t index = 0; index < browser_processes.size(); index++) {
    if (browser_processes[index].processes.empty())
      continue;

    // Sum the information for the processes within this browser.
    ProcessMemoryInformation aggregate;
    ProcessMemoryInformationList::const_iterator iterator;
    iterator = browser_processes[index].processes.begin();
    aggregate.pid = iterator->pid;
    aggregate.version = iterator->version;
    while (iterator != browser_processes[index].processes.end()) {
      if (!iterator->is_diagnostics ||
          browser_processes[index].processes.size() == 1) {
        aggregate.working_set.priv += iterator->working_set.priv;
        aggregate.working_set.shared += iterator->working_set.shared;
        aggregate.working_set.shareable += iterator->working_set.shareable;
        aggregate.committed.priv += iterator->committed.priv;
        aggregate.committed.mapped += iterator->committed.mapped;
        aggregate.committed.image += iterator->committed.image;
        aggregate.num_processes++;
      }
      ++iterator;
    }
    DictionaryValue* browser_data = new DictionaryValue();
    browsers->Append(browser_data);
    browser_data->SetString("name", browser_processes[index].name);

    BindProcessMetrics(browser_data, &aggregate);

    // We log memory info as we record it.
    if (log_string.length() > 0)
      log_string.append(L", ");
    log_string.append(UTF16ToWide(browser_processes[index].name));
    log_string.append(L", ");
    log_string.append(UTF8ToWide(
        base::Int64ToString(aggregate.working_set.priv)));
    log_string.append(L", ");
    log_string.append(UTF8ToWide(
        base::Int64ToString(aggregate.working_set.shared)));
    log_string.append(L", ");
    log_string.append(UTF8ToWide(
        base::Int64ToString(aggregate.working_set.shareable)));
  }
  if (log_string.length() > 0)
    VLOG(1) << "memory: " << log_string;

  // Set the browser & renderer detailed process data.
  DictionaryValue* browser_data = new DictionaryValue();
  root.Set("browzr_data", browser_data);
  ListValue* child_data = new ListValue();
  root.Set("child_data", child_data);

  ProcessData process = browser_processes[0];  // Chrome is the first browser.
  root.SetString("current_browser_name", process.name);

  for (size_t index = 0; index < process.processes.size(); index++) {
    if (process.processes[index].type == content::PROCESS_TYPE_BROWSER)
      BindProcessMetrics(browser_data, &process.processes[index]);
    else
      AppendProcess(child_data, &process.processes[index]);
  }

  root.SetBoolean("show_other_browsers",
      browser_defaults::kShowOtherBrowsersInAboutMemory);

  std::string data;
  jstemplate_builder::AppendJsonJS(&root, &data);
  source_->FinishDataRequest(data, request_id_);
}

#if defined(OS_CHROMEOS)
// ChromeOSAboutVersionHandler  -----------------------------------------------

ChromeOSAboutVersionHandler::ChromeOSAboutVersionHandler(
    AboutUIHTMLSource* source,
    int request_id)
    : source_(source),
      request_id_(request_id) {
  loader_.GetVersion(&consumer_,
                     base::Bind(&ChromeOSAboutVersionHandler::OnVersion,
                                base::Unretained(this)),
                     chromeos::VersionLoader::VERSION_FULL);
}

void ChromeOSAboutVersionHandler::OnVersion(
    chromeos::VersionLoader::Handle handle,
    std::string version) {
  DictionaryValue localized_strings;
  localized_strings.SetString("os_version", version);
  source_->FinishDataRequest(AboutVersionStrings(
      &localized_strings, source_->profile()), request_id_);

  // CancelableRequestProvider isn't happy when it's deleted and servicing a
  // task, so we delay the deletion.
  MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

#endif

}  // namespace

// AboutUIHTMLSource ----------------------------------------------------------

AboutUIHTMLSource::AboutUIHTMLSource(const std::string& source_name,
                                     Profile* profile)
    : DataSource(source_name, MessageLoop::current()),
      profile_(profile) {
}

AboutUIHTMLSource::~AboutUIHTMLSource() {
}

void AboutUIHTMLSource::StartDataRequest(const std::string& path,
                                         bool is_incognito,
                                         int request_id) {
  std::string response;
  std::string host = source_name();
  // Add your data source here, in alphabetical order.
  if (host == chrome::kChromeUIChromeURLsHost) {
    response = ChromeURLs();
  } else if (host == chrome::kChromeUICreditsHost) {
    int idr = (path == kCreditsJsPath) ? IDR_CREDITS_JS : IDR_CREDITS_HTML;
    response = ResourceBundle::GetSharedInstance().GetRawDataResource(
        idr).as_string();
#if defined(OS_CHROMEOS)
  } else if (host == chrome::kChromeUICryptohomeHost) {
    FinishCryptohomeDataRequest(this, path, request_id);
    return;
  } else if (host == chrome::kChromeUIDiscardsHost) {
    response = AboutDiscards(path);
#endif
#if defined(USE_ASH)
  } else if (host == chrome::kChromeUITransparencyHost) {
    response = AboutTransparency(path);
#endif
  } else if (host == chrome::kChromeUIDNSHost) {
    AboutDnsHandler::Start(this, request_id);
    return;
  } else if (host == chrome::kChromeUIHistogramsHost) {
    response = AboutHistograms(path);
#if defined(OS_LINUX) || defined(OS_OPENBSD)
  } else if (host == chrome::kChromeUILinuxProxyConfigHost) {
    response = AboutLinuxProxyConfig();
#endif
  } else if (host == chrome::kChromeUIMemoryHost) {
    response = GetAboutMemoryRedirectResponse(profile());
  } else if (host == chrome::kChromeUIMemoryRedirectHost) {
    FinishMemoryDataRequest(path, this, request_id);
    return;
#if defined(OS_CHROMEOS)
  } else if (host == chrome::kChromeUINetworkHost) {
    response = AboutNetwork(path);
  } else if (host == chrome::kChromeUIOSCreditsHost) {
    response = ResourceBundle::GetSharedInstance().GetRawDataResource(
        IDR_OS_CREDITS_HTML).as_string();
#endif
#if defined(OS_LINUX) || defined(OS_OPENBSD)
  } else if (host == chrome::kChromeUISandboxHost) {
    response = AboutSandbox();
#endif
  } else if (host == chrome::kChromeUIStatsHost) {
    response = AboutStats(path);
  } else if (host == chrome::kChromeUITermsHost) {
#if defined(OS_CHROMEOS)
    ChromeOSTermsHandler::Start(this, path, request_id);
    return;
#else
    response = ResourceBundle::GetSharedInstance().GetRawDataResource(
        IDR_TERMS_HTML).as_string();
#endif
  } else if (host == chrome::kChromeUIVersionHost) {
    if (path == kStringsJsPath) {
      // The Flash version information is needed on this page, so make sure
      // the plugins are loaded.
      PluginService::GetInstance()->GetPlugins(
          base::Bind(&HandleAboutVersionStrings,
                     make_scoped_refptr(this), request_id));
      return;
    } else {
      response = AboutVersionStaticContent(path);
    }
  }

  FinishDataRequest(response, request_id);
}

void AboutUIHTMLSource::FinishDataRequest(const std::string& html,
                                          int request_id) {
  std::string html_copy(html);
  SendResponse(request_id, base::RefCountedString::TakeString(&html_copy));
}

std::string AboutUIHTMLSource::GetMimeType(const std::string& path) const {
  if (path == kCreditsJsPath ||
      path == kStatsJsPath   ||
      path == kStringsJsPath ||
      path == kVersionJsPath ||
      path == kMemoryJsPath) {
    return "application/javascript";
  }
  return "text/html";
}

AboutUI::AboutUI(content::WebUI* web_ui, const std::string& name)
    : WebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  ChromeURLDataManager::DataSource* source =
      new AboutUIHTMLSource(name, profile);
  if (source) {
    ChromeURLDataManager::AddDataSource(profile, source);
  }
}
