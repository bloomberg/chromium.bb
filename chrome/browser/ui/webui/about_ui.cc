// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/about_ui.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/format_macros.h"
#include "base/i18n/number_formatting.h"
#include "base/json/json_writer.h"
#include "base/macros.h"
#include "base/memory/singleton.h"
#include "base/metrics/statistics_recorder.h"
#include "base/process/process_metrics.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/sys_info.h"
#include "base/task_scheduler/post_task.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/net/predictor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/resource_coordinator/tab_manager.h"
#include "chrome/browser/resource_coordinator/tab_stats.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/about_ui/credit_utils.h"
#include "components/grit/components_resources.h"
#include "components/strings/grit/components_locale_settings.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/process_type.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/escape.h"
#include "net/base/filename_util.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "third_party/brotli/include/brotli/decode.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/jstemplate_builder.h"
#include "ui/base/webui/web_ui_util.h"
#include "url/gurl.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/ui/webui/theme_source.h"
#endif

#if defined(OS_WIN)
#include "chrome/browser/win/enumerate_modules_model.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/browser_process_platform_part_chromeos.h"
#include "chrome/browser/chromeos/customization/customization_document.h"
#endif

using base::Time;
using base::TimeDelta;
using content::BrowserThread;
using content::WebContents;

namespace {

constexpr char kCreditsJsPath[] = "credits.js";
constexpr char kStatsJsPath[] = "stats.js";
constexpr char kStringsJsPath[] = "strings.js";

#if defined(OS_CHROMEOS)

constexpr char kKeyboardUtilsPath[] = "keyboard_utils.js";

// Loads bundled Eula contents. The online version of Eula is fetched in Eula
// screen javascript. This is intentional because chrome://terms runs in a
// privileged webui context and should never load from untrusted places.
class ChromeOSTermsHandler
    : public base::RefCountedThreadSafe<ChromeOSTermsHandler> {
 public:
  static void Start(const std::string& path,
                    const content::URLDataSource::GotDataCallback& callback) {
    scoped_refptr<ChromeOSTermsHandler> handler(
        new ChromeOSTermsHandler(path, callback));
    handler->StartOnUIThread();
  }

 private:
  friend class base::RefCountedThreadSafe<ChromeOSTermsHandler>;

  ChromeOSTermsHandler(const std::string& path,
                       const content::URLDataSource::GotDataCallback& callback)
    : path_(path),
      callback_(callback),
      // Previously we were using "initial locale" http://crbug.com/145142
      locale_(g_browser_process->GetApplicationLocale()) {
  }

  virtual ~ChromeOSTermsHandler() {}

  void StartOnUIThread() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (path_ == chrome::kOemEulaURLPath) {
      // Load local OEM EULA from the disk.
      base::PostTaskWithTraitsAndReply(
          FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
          base::BindOnce(&ChromeOSTermsHandler::LoadOemEulaFileAsync, this),
          base::BindOnce(&ChromeOSTermsHandler::ResponseOnUIThread, this));
    } else {
      // Load local ChromeOS terms from the file.
      base::PostTaskWithTraitsAndReply(
          FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
          base::BindOnce(&ChromeOSTermsHandler::LoadEulaFileAsync, this),
          base::BindOnce(&ChromeOSTermsHandler::ResponseOnUIThread, this));
    }
  }

  void LoadOemEulaFileAsync() {
    base::AssertBlockingAllowed();

    const chromeos::StartupCustomizationDocument* customization =
        chromeos::StartupCustomizationDocument::GetInstance();
    if (!customization->IsReady())
      return;

    base::FilePath oem_eula_file_path;
    if (net::FileURLToFilePath(GURL(customization->GetEULAPage(locale_)),
                               &oem_eula_file_path)) {
      if (!base::ReadFileToString(oem_eula_file_path, &contents_)) {
        contents_.clear();
      }
    }
  }

  void LoadEulaFileAsync() {
    base::AssertBlockingAllowed();

    std::string file_path =
        base::StringPrintf(chrome::kEULAPathFormat, locale_.c_str());
    if (!base::ReadFileToString(base::FilePath(file_path), &contents_)) {
      // No EULA for given language - try en-US as default.
      file_path = base::StringPrintf(chrome::kEULAPathFormat, "en-US");
      if (!base::ReadFileToString(base::FilePath(file_path), &contents_)) {
        // File with EULA not found, ResponseOnUIThread will load EULA from
        // resources if contents_ is empty.
        contents_.clear();
      }
    }
  }

  void ResponseOnUIThread() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    // If we fail to load Chrome OS EULA from disk, load it from resources.
    // Do nothing if OEM EULA load failed.
    if (contents_.empty() && path_ != chrome::kOemEulaURLPath)
      contents_ = l10n_util::GetStringUTF8(IDS_TERMS_HTML);
    callback_.Run(base::RefCountedString::TakeString(&contents_));
  }

  // Path in the URL.
  const std::string path_;

  // Callback to run with the response.
  content::URLDataSource::GotDataCallback callback_;

  // Locale of the EULA.
  const std::string locale_;

  // EULA contents that was loaded from file.
  std::string contents_;

  DISALLOW_COPY_AND_ASSIGN(ChromeOSTermsHandler);
};

class ChromeOSCreditsHandler
    : public base::RefCountedThreadSafe<ChromeOSCreditsHandler> {
 public:
  static void Start(const std::string& path,
                    const content::URLDataSource::GotDataCallback& callback) {
    scoped_refptr<ChromeOSCreditsHandler> handler(
        new ChromeOSCreditsHandler(path, callback));
    handler->StartOnUIThread();
  }

 private:
  friend class base::RefCountedThreadSafe<ChromeOSCreditsHandler>;

  ChromeOSCreditsHandler(
      const std::string& path,
      const content::URLDataSource::GotDataCallback& callback)
      : path_(path), callback_(callback) {}

  virtual ~ChromeOSCreditsHandler() {}

  void StartOnUIThread() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (path_ == kKeyboardUtilsPath) {
      contents_ = ui::ResourceBundle::GetSharedInstance()
                      .GetRawDataResource(IDR_KEYBOARD_UTILS_JS)
                      .as_string();
      ResponseOnUIThread();
      return;
    }
    // Load local Chrome OS credits from the disk.
    base::PostTaskWithTraitsAndReply(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::BACKGROUND},
        base::Bind(&ChromeOSCreditsHandler::LoadCreditsFileAsync, this),
        base::Bind(&ChromeOSCreditsHandler::ResponseOnUIThread, this));
  }

  void LoadCreditsFileAsync() {
    base::FilePath credits_file_path(chrome::kChromeOSCreditsPath);
    if (!base::ReadFileToString(credits_file_path, &contents_)) {
      // File with credits not found, ResponseOnUIThread will load credits
      // from resources if contents_ is empty.
      contents_.clear();
    }
  }

  void ResponseOnUIThread() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    // If we fail to load Chrome OS credits from disk, load it from resources.
    if (contents_.empty() && path_ != kKeyboardUtilsPath) {
      contents_ = ui::ResourceBundle::GetSharedInstance()
                      .GetRawDataResource(IDR_OS_CREDITS_HTML)
                      .as_string();
    }
    callback_.Run(base::RefCountedString::TakeString(&contents_));
  }

  // Path in the URL.
  const std::string path_;

  // Callback to run with the response.
  content::URLDataSource::GotDataCallback callback_;

  // Chrome OS credits contents that was loaded from file.
  std::string contents_;

  DISALLOW_COPY_AND_ASSIGN(ChromeOSCreditsHandler);
};
#endif

}  // namespace

// Individual about handlers ---------------------------------------------------

namespace about_ui {

void AppendHeader(std::string* output, int refresh,
                  const std::string& unescaped_title) {
  output->append("<!DOCTYPE HTML>\n<html>\n<head>\n");
  if (!unescaped_title.empty()) {
    output->append("<title>");
    output->append(net::EscapeForHTML(unescaped_title));
    output->append("</title>\n");
  }
  output->append("<meta charset='utf-8'>\n");
  if (refresh > 0) {
    output->append("<meta http-equiv='refresh' content='");
    output->append(base::IntToString(refresh));
    output->append("'/>\n");
  }
}

void AppendBody(std::string *output) {
  output->append("</head>\n<body>\n");
}

void AppendFooter(std::string *output) {
  output->append("</body>\n</html>\n");
}

}  // namespace about_ui

using about_ui::AppendHeader;
using about_ui::AppendBody;
using about_ui::AppendFooter;

namespace {

std::string ChromeURLs() {
  std::string html;
  AppendHeader(&html, 0, "Chrome URLs");
  AppendBody(&html);
  html += "<h2>List of Chrome URLs</h2>\n<ul>\n";
  std::vector<std::string> hosts(
      chrome::kChromeHostURLs,
      chrome::kChromeHostURLs + chrome::kNumberOfChromeHostURLs);
  std::sort(hosts.begin(), hosts.end());
  for (std::vector<std::string>::const_iterator i = hosts.begin();
       i != hosts.end(); ++i)
    html += "<li><a href='chrome://" + *i + "/'>chrome://" + *i + "</a></li>\n";
  html += "</ul>\n<h2>For Debug</h2>\n"
      "<p>The following pages are for debugging purposes only. Because they "
      "crash or hang the renderer, they're not linked directly; you can type "
      "them into the address bar if you need them.</p>\n<ul>";
  for (size_t i = 0; i < chrome::kNumberOfChromeDebugURLs; i++)
    html += "<li>" + std::string(chrome::kChromeDebugURLs[i]) + "</li>\n";
  html += "</ul>\n";
  AppendFooter(&html);
  return html;
}

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX)

const char kAboutDiscardsRunCommand[] = "run";
const char kAboutDiscardsSkipUnloadHandlersCommand[] = "skip_unload_handlers";

// Html output helper functions

// Helper function to wrap HTML with a tag.
std::string WrapWithTag(const std::string& tag, const std::string& text) {
  return "<" + tag + ">" + text + "</" + tag + ">";
}

// Helper function to wrap Html with <td> tag.
std::string WrapWithTD(const std::string& text) {
  return "<td>" + text + "</td>";
}

// Helper function to wrap Html with <tr> tag.
std::string WrapWithTR(const std::string& text) {
  return "<tr>" + text + "</tr>";
}

std::string AddStringRow(const std::string& name, const std::string& value) {
  std::string row;
  row.append(WrapWithTD(name));
  row.append(WrapWithTD(value));
  return WrapWithTR(row);
}

void AddContentSecurityPolicy(std::string* output) {
  output->append("<meta http-equiv='Content-Security-Policy' "
      "content='default-src 'none';'>");
}

// TODO(stevenjb): L10N AboutDiscards.

std::string BuildAboutDiscardsRunPage() {
  std::string output;
  AppendHeader(&output, 0, "About discards");
  output.append(base::StringPrintf("<meta http-equiv='refresh' content='2;%s'>",
                                   chrome::kChromeUIDiscardsURL));
  AddContentSecurityPolicy(&output);
  output.append(WrapWithTag("p", "Discarding a tab..."));
  AppendFooter(&output);
  return output;
}

std::vector<std::string> GetHtmlTabDescriptorsForDiscardPage() {
  resource_coordinator::TabManager* tab_manager =
      g_browser_process->GetTabManager();
  resource_coordinator::TabStatsList stats = tab_manager->GetTabStats();
  std::vector<std::string> titles;
  titles.reserve(stats.size());
  for (resource_coordinator::TabStatsList::iterator it = stats.begin();
       it != stats.end(); ++it) {
    std::string str;
    str.reserve(4096);
    str += "<b>";
    str += it->is_app ? "[App] " : "";
    str += it->is_internal_page ? "[Internal] " : "";
    str += it->is_media ? "[Media] " : "";
    str += it->is_pinned ? "[Pinned] " : "";
    str += it->is_discarded ? "[Discarded] " : "";
    str += "</b>";
    str += net::EscapeForHTML(base::UTF16ToUTF8(it->title));
#if defined(OS_CHROMEOS)
    str += base::StringPrintf(" (%d) ", it->oom_score);
#endif
    str += base::StringPrintf("&nbsp;&nbsp;(%d discards this session)",
                              it->discard_count);

    if (!it->is_discarded) {
      str += "<ul>";
      str += base::StringPrintf("<li><a href='%s%s/%" PRId64
                                "'>Discard (safely)</a></li>",
                                chrome::kChromeUIDiscardsURL,
                                kAboutDiscardsRunCommand, it->tab_contents_id);
      str += base::StringPrintf(
          "<li><a href='%s%s/%" PRId64
          "?%s'>Discard (allow unsafe process shutdown)</a></li>",
          chrome::kChromeUIDiscardsURL, kAboutDiscardsRunCommand,
          it->tab_contents_id, kAboutDiscardsSkipUnloadHandlersCommand);
      str += "</ul>";
    }
    titles.push_back(str);
  }
  return titles;
}

std::string AboutDiscards(const std::string& path) {
  std::string output;
  int64_t web_content_id;
  resource_coordinator::TabManager* tab_manager =
      g_browser_process->GetTabManager();

  std::vector<std::string> url_split =
      base::SplitString(path, "?", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (!url_split.empty()) {
    resource_coordinator::TabManager::DiscardTabCondition discard_condition;
    if ((url_split.size() > 1 &&
         url_split[1] == kAboutDiscardsSkipUnloadHandlersCommand)) {
      discard_condition = resource_coordinator::TabManager::kUrgentShutdown;
    } else {
      discard_condition = resource_coordinator::TabManager::kProactiveShutdown;
    }

    std::vector<std::string> path_split = base::SplitString(
        url_split[0], "/", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    if (path_split.size() == 2 && path_split[0] == kAboutDiscardsRunCommand &&
        base::StringToInt64(path_split[1], &web_content_id)) {
      tab_manager->DiscardTabById(web_content_id, discard_condition);
      return BuildAboutDiscardsRunPage();
    } else if (path_split.size() == 1 &&
               path_split[0] == kAboutDiscardsRunCommand) {
      tab_manager->DiscardTab(discard_condition);
      return BuildAboutDiscardsRunPage();
    }
  }

  AppendHeader(&output, 0, "About discards");
  AddContentSecurityPolicy(&output);
  AppendBody(&output);
  output.append("<h3>Discarded Tabs</h3>");
  output.append(
      "<p>Tabs sorted from most interesting to least interesting. The least "
      "interesting tab may be discarded if we run out of physical memory.</p>");

  std::vector<std::string> titles = GetHtmlTabDescriptorsForDiscardPage();
  if (!titles.empty()) {
    output.append("<ul>");
    std::vector<std::string>::iterator it = titles.begin();
    for ( ; it != titles.end(); ++it) {
      output.append(WrapWithTag("li", *it));
    }
    output.append("</ul>");
  } else {
    output.append("<p>None found.  Wait 10 seconds, then refresh.</p>");
  }
  output.append(base::StringPrintf("%d discards this session. ",
                                   tab_manager->discard_count()));
  output.append(base::StringPrintf(
      "<a href='%s%s'>Discard tab now (safely)</a>",
      chrome::kChromeUIDiscardsURL, kAboutDiscardsRunCommand));

  base::SystemMemoryInfoKB meminfo;
  base::GetSystemMemoryInfo(&meminfo);
  output.append("<h3>System memory information in MB</h3>");
  output.append("<table>");
  // Start with summary statistics.
  output.append(AddStringRow(
      "Total", base::IntToString(meminfo.total / 1024)));
  output.append(AddStringRow(
      "Free",
      base::IntToString(base::SysInfo::AmountOfAvailablePhysicalMemory() /
                        1024 / 1024)));
#if defined(OS_CHROMEOS)
  int mem_allocated_kb = meminfo.active_anon + meminfo.inactive_anon;
#if defined(ARCH_CPU_ARM_FAMILY)
  // ARM counts allocated graphics memory separately from anonymous.
  if (meminfo.gem_size != -1)
    mem_allocated_kb += meminfo.gem_size / 1024;
#endif
  output.append(AddStringRow(
      "Allocated", base::IntToString(mem_allocated_kb / 1024)));
  // Add some space, then detailed numbers.
  output.append(AddStringRow("&nbsp;", "&nbsp;"));
  output.append(AddStringRow(
      "Buffered", base::IntToString(meminfo.buffers / 1024)));
  output.append(AddStringRow(
      "Cached", base::IntToString(meminfo.cached / 1024)));
  output.append(AddStringRow(
      "Active Anon", base::IntToString(meminfo.active_anon / 1024)));
  output.append(AddStringRow(
      "Inactive Anon", base::IntToString(meminfo.inactive_anon / 1024)));
  output.append(AddStringRow(
      "Shared", base::IntToString(meminfo.shmem / 1024)));
  output.append(AddStringRow(
      "Graphics", base::IntToString(meminfo.gem_size / 1024 / 1024)));
#endif  // OS_CHROMEOS
  output.append("</table>");
  AppendFooter(&output);
  return output;
}

#endif  // OS_WIN || OS_MACOSX || OS_LINUX

// AboutDnsHandler bounces the request back to the IO thread to collect
// the DNS information.
class AboutDnsHandler : public base::RefCountedThreadSafe<AboutDnsHandler> {
 public:
  static void Start(Profile* profile,
                    const content::URLDataSource::GotDataCallback& callback) {
    scoped_refptr<AboutDnsHandler> handler(
        new AboutDnsHandler(profile, callback));
    handler->StartOnUIThread();
  }

 private:
  friend class base::RefCountedThreadSafe<AboutDnsHandler>;

  AboutDnsHandler(Profile* profile,
                  const content::URLDataSource::GotDataCallback& callback)
      : profile_(profile),
        callback_(callback) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
  }

  virtual ~AboutDnsHandler() {}

  // Calls FinishOnUIThread() on completion.
  void StartOnUIThread() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    chrome_browser_net::Predictor* predictor = profile_->GetNetworkPredictor();
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::BindOnce(&AboutDnsHandler::StartOnIOThread, this, predictor));
  }

  void StartOnIOThread(chrome_browser_net::Predictor* predictor) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);

    std::string data;
    AppendHeader(&data, 0, "About DNS");
    AppendBody(&data);
    chrome_browser_net::Predictor::PredictorGetHtmlInfo(predictor, &data);
    AppendFooter(&data);

    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::BindOnce(&AboutDnsHandler::FinishOnUIThread, this, data));
  }

  void FinishOnUIThread(const std::string& data) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    std::string data_copy(data);
    callback_.Run(base::RefCountedString::TakeString(&data_copy));
  }

  Profile* profile_;

  // Callback to run with the response.
  content::URLDataSource::GotDataCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(AboutDnsHandler);
};

#if defined(OS_LINUX) || defined(OS_OPENBSD)
std::string AboutLinuxProxyConfig() {
  std::string data;
  AppendHeader(&data, 0,
               l10n_util::GetStringUTF8(IDS_ABOUT_LINUX_PROXY_CONFIG_TITLE));
  data.append("<style>body { max-width: 70ex; padding: 2ex 5ex; }</style>");
  AppendBody(&data);
  base::FilePath binary = base::CommandLine::ForCurrentProcess()->GetProgram();
  data.append(
      l10n_util::GetStringFUTF8(IDS_ABOUT_LINUX_PROXY_CONFIG_BODY,
                                l10n_util::GetStringUTF16(IDS_PRODUCT_NAME),
                                base::ASCIIToUTF16(binary.BaseName().value())));
  AppendFooter(&data);
  return data;
}
#endif

}  // namespace

// AboutUIHTMLSource ----------------------------------------------------------

AboutUIHTMLSource::AboutUIHTMLSource(const std::string& source_name,
                                     Profile* profile)
    : source_name_(source_name),
      profile_(profile) {}

AboutUIHTMLSource::~AboutUIHTMLSource() {}

std::string AboutUIHTMLSource::GetSource() const {
  return source_name_;
}

void AboutUIHTMLSource::StartDataRequest(
    const std::string& path,
    const content::ResourceRequestInfo::WebContentsGetter& wc_getter,
    const content::URLDataSource::GotDataCallback& callback) {
  std::string response;
  // Add your data source here, in alphabetical order.
  if (source_name_ == chrome::kChromeUIChromeURLsHost) {
    response = ChromeURLs();
  } else if (source_name_ == chrome::kChromeUICreditsHost) {
    int idr = IDR_ABOUT_UI_CREDITS_HTML;
    if (path == kCreditsJsPath)
      idr = IDR_ABOUT_UI_CREDITS_JS;
#if defined(OS_CHROMEOS)
    else if (path == kKeyboardUtilsPath)
      idr = IDR_KEYBOARD_UTILS_JS;
#endif

    if (idr == IDR_ABOUT_UI_CREDITS_HTML) {
      response = about_ui::GetCredits(true /*include_scripts*/);
    } else {
      response = ui::ResourceBundle::GetSharedInstance()
                     .GetRawDataResource(idr)
                     .as_string();
    }

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX)
  } else if (source_name_ == chrome::kChromeUIDiscardsHost) {
    response = AboutDiscards(path);
#endif
  } else if (source_name_ == chrome::kChromeUIDNSHost) {
    AboutDnsHandler::Start(profile(), callback);
    return;
#if defined(OS_LINUX) || defined(OS_OPENBSD)
  } else if (source_name_ == chrome::kChromeUILinuxProxyConfigHost) {
    response = AboutLinuxProxyConfig();
#endif
#if defined(OS_CHROMEOS)
  } else if (source_name_ == chrome::kChromeUIOSCreditsHost) {
    ChromeOSCreditsHandler::Start(path, callback);
    return;
#endif
#if !defined(OS_ANDROID)
  } else if (source_name_ == chrome::kChromeUITermsHost) {
#if defined(OS_CHROMEOS)
    ChromeOSTermsHandler::Start(path, callback);
    return;
#else
    response = l10n_util::GetStringUTF8(IDS_TERMS_HTML);
#endif
#endif
  }

  FinishDataRequest(response, callback);
}

void AboutUIHTMLSource::FinishDataRequest(
    const std::string& html,
    const content::URLDataSource::GotDataCallback& callback) {
  std::string html_copy(html);
  callback.Run(base::RefCountedString::TakeString(&html_copy));
}

std::string AboutUIHTMLSource::GetMimeType(const std::string& path) const {
  if (path == kCreditsJsPath     ||
#if defined(OS_CHROMEOS)
      path == kKeyboardUtilsPath ||
#endif
      path == kStatsJsPath       ||
      path == kStringsJsPath) {
    return "application/javascript";
  }
  return "text/html";
}

bool AboutUIHTMLSource::ShouldAddContentSecurityPolicy() const {
#if defined(OS_CHROMEOS)
  if (source_name_ == chrome::kChromeUIOSCreditsHost)
    return false;
#endif
  return content::URLDataSource::ShouldAddContentSecurityPolicy();
}

std::string AboutUIHTMLSource::GetAccessControlAllowOriginForOrigin(
    const std::string& origin) const {
#if defined(OS_CHROMEOS)
  // Allow chrome://oobe to load chrome://terms via XHR.
  if (source_name_ == chrome::kChromeUITermsHost &&
      base::StartsWith(chrome::kChromeUIOobeURL, origin,
                       base::CompareCase::SENSITIVE)) {
    return origin;
  }
#endif
  return content::URLDataSource::GetAccessControlAllowOriginForOrigin(origin);
}

AboutUI::AboutUI(content::WebUI* web_ui, const std::string& name)
    : WebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);

#if !defined(OS_ANDROID)
  // Set up the chrome://theme/ source.
  ThemeSource* theme = new ThemeSource(profile);
  content::URLDataSource::Add(profile, theme);
#endif

  content::URLDataSource::Add(profile, new AboutUIHTMLSource(name, profile));
}
