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
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/memory/tab_manager.h"
#include "chrome/browser/memory/tab_stats.h"
#include "chrome/browser/memory_details.h"
#include "chrome/browser/net/predictor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/locale_settings.h"
#include "components/strings/grit/components_locale_settings.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/process_type.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "grit/browser_resources.h"
#include "grit/components_resources.h"
#include "net/base/escape.h"
#include "net/base/filename_util.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_status.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/jstemplate_builder.h"
#include "ui/base/webui/web_ui_util.h"
#include "url/gurl.h"

#if defined(ENABLE_THEMES)
#include "chrome/browser/ui/webui/theme_source.h"
#endif

#if defined(OS_LINUX) || defined(OS_OPENBSD)
#include "content/public/browser/zygote_host_linux.h"
#include "content/public/common/sandbox_linux.h"
#endif

#if defined(OS_WIN)
#include "chrome/browser/enumerate_modules_model_win.h"
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

const char kCreditsJsPath[] = "credits.js";
const char kMemoryJsPath[] = "memory.js";
const char kMemoryCssPath[] = "about_memory.css";
const char kStatsJsPath[] = "stats.js";
const char kStringsJsPath[] = "strings.js";

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
  return base::StringPrintf("<meta http-equiv='refresh' content='0;%s'>",
                            chrome::kChromeUIMemoryRedirectURL);
}

// Handling about:memory is complicated enough to encapsulate its related
// methods into a single class. The user should create it (on the heap) and call
// its |StartFetch()| method.
class AboutMemoryHandler : public MemoryDetails {
 public:
  explicit AboutMemoryHandler(
      const content::URLDataSource::GotDataCallback& callback)
      : callback_(callback) {
  }

  void OnDetailsAvailable() override;

 private:
  ~AboutMemoryHandler() override {}

  void BindProcessMetrics(base::DictionaryValue* data,
                          ProcessMemoryInformation* info);
  void AppendProcess(base::ListValue* child_data,
                     ProcessMemoryInformation* info);

  content::URLDataSource::GotDataCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(AboutMemoryHandler);
};

#if defined(OS_CHROMEOS)

const char kKeyboardUtilsPath[] = "keyboard_utils.js";

// chrome://terms falls back to offline page after kOnlineTermsTimeoutSec.
const int kOnlineTermsTimeoutSec = 7;

// Helper class that fetches the online Chrome OS terms. Empty string is
// returned once fetching failed or exceeded |kOnlineTermsTimeoutSec|.
class ChromeOSOnlineTermsHandler : public net::URLFetcherDelegate {
 public:
  typedef base::Callback<void (ChromeOSOnlineTermsHandler*)> FetchCallback;

  explicit ChromeOSOnlineTermsHandler(const FetchCallback& callback,
                                      const std::string& locale)
      : fetch_callback_(callback) {
    std::string eula_URL = base::StringPrintf(chrome::kOnlineEulaURLPath,
                                              locale.c_str());
    eula_fetcher_ =
        net::URLFetcher::Create(0 /* ID used for testing */, GURL(eula_URL),
                                net::URLFetcher::GET, this);
    eula_fetcher_->SetRequestContext(
        g_browser_process->system_request_context());
    eula_fetcher_->AddExtraRequestHeader("Accept: text/html");
    eula_fetcher_->SetLoadFlags(net::LOAD_DO_NOT_SEND_COOKIES |
                                net::LOAD_DO_NOT_SAVE_COOKIES |
                                net::LOAD_DISABLE_CACHE);
    eula_fetcher_->Start();
    // Abort the download attempt if it takes longer than one minute.
    download_timer_.Start(FROM_HERE,
                          base::TimeDelta::FromSeconds(kOnlineTermsTimeoutSec),
                          this,
                          &ChromeOSOnlineTermsHandler::OnDownloadTimeout);
  }

  void GetResponseResult(std::string* response_string) {
    std::string mime_type;
    if (!eula_fetcher_ ||
        !eula_fetcher_->GetStatus().is_success() ||
        eula_fetcher_->GetResponseCode() != 200 ||
        !eula_fetcher_->GetResponseHeaders()->GetMimeType(&mime_type) ||
        mime_type != "text/html" ||
        !eula_fetcher_->GetResponseAsString(response_string)) {
      response_string->clear();
    }
  }

 private:
  // Prevents allocation on the stack. ChromeOSOnlineTermsHandler should be
  // created by 'operator new'. |this| takes care of destruction.
  ~ChromeOSOnlineTermsHandler() override {}

  // net::URLFetcherDelegate:
  void OnURLFetchComplete(const net::URLFetcher* source) override {
    if (source != eula_fetcher_.get()) {
      NOTREACHED() << "Callback from foreign URL fetcher";
      return;
    }
    fetch_callback_.Run(this);
    delete this;
  }

  void OnDownloadTimeout() {
    eula_fetcher_.reset();
    fetch_callback_.Run(this);
    delete this;
  }

  // Timer that enforces a timeout on the attempt to download the
  // ChromeOS Terms.
  base::OneShotTimer download_timer_;

  // |fetch_callback_| called when fetching succeeded or failed.
  FetchCallback fetch_callback_;

  // Helper to fetch online eula.
  scoped_ptr<net::URLFetcher> eula_fetcher_;

  DISALLOW_COPY_AND_ASSIGN(ChromeOSOnlineTermsHandler);
};

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
      BrowserThread::PostTask(
          BrowserThread::FILE, FROM_HERE,
          base::Bind(&ChromeOSTermsHandler::LoadOemEulaFileOnFileThread, this));
    } else {
      // Try to load online version of ChromeOS terms first.
      // ChromeOSOnlineTermsHandler object destroys itself.
      new ChromeOSOnlineTermsHandler(
          base::Bind(&ChromeOSTermsHandler::OnOnlineEULAFetched, this),
          locale_);
    }
  }

  void OnOnlineEULAFetched(ChromeOSOnlineTermsHandler* loader) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    loader->GetResponseResult(&contents_);
    if (contents_.empty()) {
      // Load local ChromeOS terms from the file.
      BrowserThread::PostTask(
          BrowserThread::FILE, FROM_HERE,
          base::Bind(&ChromeOSTermsHandler::LoadEulaFileOnFileThread, this));
    } else {
      ResponseOnUIThread();
    }
  }

  void LoadOemEulaFileOnFileThread() {
    DCHECK_CURRENTLY_ON(BrowserThread::FILE);
    const chromeos::StartupCustomizationDocument* customization =
        chromeos::StartupCustomizationDocument::GetInstance();
    if (customization->IsReady()) {
      base::FilePath oem_eula_file_path;
      if (net::FileURLToFilePath(GURL(customization->GetEULAPage(locale_)),
                                 &oem_eula_file_path)) {
        if (!base::ReadFileToString(oem_eula_file_path, &contents_)) {
          contents_.clear();
        }
      }
    }
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&ChromeOSTermsHandler::ResponseOnUIThread, this));
  }

  void LoadEulaFileOnFileThread() {
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
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&ChromeOSTermsHandler::ResponseOnUIThread, this));
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
      contents_ = ResourceBundle::GetSharedInstance()
                      .GetRawDataResource(IDR_KEYBOARD_UTILS_JS)
                      .as_string();
      ResponseOnUIThread();
      return;
    }
    // Load local Chrome OS credits from the disk.
    BrowserThread::PostBlockingPoolTaskAndReply(
        FROM_HERE,
        base::Bind(&ChromeOSCreditsHandler::LoadCreditsFileOnBlockingPool,
                   this),
        base::Bind(&ChromeOSCreditsHandler::ResponseOnUIThread, this));
  }

  void LoadCreditsFileOnBlockingPool() {
    DCHECK(BrowserThread::GetBlockingPool()->RunsTasksOnCurrentThread());
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
      contents_ = ResourceBundle::GetSharedInstance()
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
  for (int i = 0; i < chrome::kNumberOfChromeDebugURLs; i++)
    html += "<li>" + std::string(chrome::kChromeDebugURLs[i]) + "</li>\n";
  html += "</ul>\n";
  AppendFooter(&html);
  return html;
}

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_CHROMEOS)

const char kAboutDiscardsRunCommand[] = "run";

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
  memory::TabManager* tab_manager = g_browser_process->GetTabManager();
  memory::TabStatsList stats = tab_manager->GetTabStats();
  std::vector<std::string> titles;
  titles.reserve(stats.size());
  for (memory::TabStatsList::iterator it = stats.begin(); it != stats.end();
       ++it) {
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
    if (!it->is_discarded) {
      str += base::StringPrintf(" <a href='%s%s/%" PRId64 "'>Discard</a>",
                                chrome::kChromeUIDiscardsURL,
                                kAboutDiscardsRunCommand, it->tab_contents_id);
    }
    str += base::StringPrintf("&nbsp;&nbsp;(%d discards this session)",
                              it->discard_count);
    titles.push_back(str);
  }
  return titles;
}

std::string AboutDiscards(const std::string& path) {
  std::string output;
  int64_t web_content_id;
  memory::TabManager* tab_manager = g_browser_process->GetTabManager();

  std::vector<std::string> path_split = base::SplitString(
      path, "/", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (path_split.size() == 2 && path_split[0] == kAboutDiscardsRunCommand &&
      base::StringToInt64(path_split[1], &web_content_id)) {
    tab_manager->DiscardTabById(web_content_id);
    return BuildAboutDiscardsRunPage();
  } else if (path_split.size() == 1 &&
             path_split[0] == kAboutDiscardsRunCommand) {
    tab_manager->DiscardTab();
    return BuildAboutDiscardsRunPage();
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
  output.append(base::StringPrintf("<a href='%s%s'>Discard tab now</a>",
                                   chrome::kChromeUIDiscardsURL,
                                   kAboutDiscardsRunCommand));

  base::SystemMemoryInfoKB meminfo;
  base::GetSystemMemoryInfo(&meminfo);
  output.append("<h3>System memory information in MB</h3>");
  output.append("<table>");
  // Start with summary statistics.
  output.append(AddStringRow(
      "Total", base::IntToString(meminfo.total / 1024)));
  output.append(AddStringRow(
      "Free", base::IntToString(meminfo.free / 1024)));
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

#endif  // OS_WIN || OS_CHROMEOS

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
        base::Bind(&AboutDnsHandler::StartOnIOThread, this, predictor));
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
        base::Bind(&AboutDnsHandler::FinishOnUIThread, this, data));
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

void FinishMemoryDataRequest(
    const std::string& path,
    const content::URLDataSource::GotDataCallback& callback) {
  if (path == kStringsJsPath) {
    // The AboutMemoryHandler cleans itself up, but |StartFetch()| will want
    // the refcount to be greater than 0.
    scoped_refptr<AboutMemoryHandler> handler(new AboutMemoryHandler(callback));
    handler->StartFetch(MemoryDetails::FROM_ALL_BROWSERS);
  } else {
    int id = IDR_ABOUT_MEMORY_HTML;
    if (path == kMemoryJsPath) {
      id = IDR_ABOUT_MEMORY_JS;
    } else if (path == kMemoryCssPath) {
      id = IDR_ABOUT_MEMORY_CSS;
    }

    std::string result =
        ResourceBundle::GetSharedInstance().GetRawDataResource(id).as_string();
    callback.Run(base::RefCountedString::TakeString(&result));
  }
}

#if defined(OS_LINUX) || defined(OS_OPENBSD)
std::string AboutLinuxProxyConfig() {
  std::string data;
  AppendHeader(&data, 0,
               l10n_util::GetStringUTF8(IDS_ABOUT_LINUX_PROXY_CONFIG_TITLE));
  data.append("<style>body { max-width: 70ex; padding: 2ex 5ex; }</style>");
  AppendBody(&data);
  base::FilePath binary = base::CommandLine::ForCurrentProcess()->GetProgram();
  data.append(l10n_util::GetStringFUTF8(
      IDS_ABOUT_LINUX_PROXY_CONFIG_BODY,
      l10n_util::GetStringUTF16(IDS_PRODUCT_NAME),
      base::ASCIIToUTF16(binary.BaseName().value())));
  AppendFooter(&data);
  return data;
}

void AboutSandboxRow(std::string* data, int name_id, bool good) {
  data->append("<tr><td>");
  data->append(l10n_util::GetStringUTF8(name_id));
  if (good) {
    data->append("</td><td style='color: green;'>");
    data->append(
        l10n_util::GetStringUTF8(IDS_CONFIRM_MESSAGEBOX_YES_BUTTON_LABEL));
  } else {
    data->append("</td><td style='color: red;'>");
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

  // Get expected sandboxing status of renderers.
  const int status =
      content::ZygoteHost::GetInstance()->GetRendererSandboxStatus();

  data.append("<table>");

  AboutSandboxRow(&data, IDS_ABOUT_SANDBOX_SUID_SANDBOX,
                  status & content::kSandboxLinuxSUID);
  AboutSandboxRow(&data, IDS_ABOUT_SANDBOX_NAMESPACE_SANDBOX,
                  status & content::kSandboxLinuxUserNS);
  AboutSandboxRow(&data, IDS_ABOUT_SANDBOX_PID_NAMESPACES,
                  status & content::kSandboxLinuxPIDNS);
  AboutSandboxRow(&data, IDS_ABOUT_SANDBOX_NET_NAMESPACES,
                  status & content::kSandboxLinuxNetNS);
  AboutSandboxRow(&data, IDS_ABOUT_SANDBOX_SECCOMP_BPF_SANDBOX,
                  status & content::kSandboxLinuxSeccompBPF);
  AboutSandboxRow(&data, IDS_ABOUT_SANDBOX_SECCOMP_BPF_SANDBOX_TSYNC,
                  status & content::kSandboxLinuxSeccompTSYNC);
  AboutSandboxRow(&data, IDS_ABOUT_SANDBOX_YAMA_LSM,
                  status & content::kSandboxLinuxYama);

  data.append("</table>");

  // Require either the setuid or namespace sandbox for our first-layer sandbox.
  bool good_layer1 = (status & content::kSandboxLinuxSUID ||
                      status & content::kSandboxLinuxUserNS) &&
                     status & content::kSandboxLinuxPIDNS &&
                     status & content::kSandboxLinuxNetNS;
  // A second-layer sandbox is also required to be adequately sandboxed.
  bool good_layer2 = status & content::kSandboxLinuxSeccompBPF;
  bool good = good_layer1 && good_layer2;

  if (good) {
    data.append("<p style='color: green'>");
    data.append(l10n_util::GetStringUTF8(IDS_ABOUT_SANDBOX_OK));
  } else {
    data.append("<p style='color: red'>");
    data.append(l10n_util::GetStringUTF8(IDS_ABOUT_SANDBOX_BAD));
  }
  data.append("</p>");

  AppendFooter(&data);
  return data;
}
#endif

// AboutMemoryHandler ----------------------------------------------------------

// Helper for AboutMemory to bind results from a ProcessMetrics object
// to a DictionaryValue. Fills ws_usage and comm_usage so that the objects
// can be used in caller's scope (e.g for appending to a net total).
void AboutMemoryHandler::BindProcessMetrics(base::DictionaryValue* data,
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
void AboutMemoryHandler::AppendProcess(base::ListValue* child_data,
                                       ProcessMemoryInformation* info) {
  DCHECK(child_data && info);

  // Append a new DictionaryValue for this renderer to our list.
  base::DictionaryValue* child = new base::DictionaryValue();
  child_data->Append(child);
  BindProcessMetrics(child, info);

  std::string child_label(
      ProcessMemoryInformation::GetFullTypeNameInEnglish(info->process_type,
                                                         info->renderer_type));
  if (info->is_diagnostics)
    child_label.append(" (diagnostics)");
  child->SetString("child_name", child_label);
  base::ListValue* titles = new base::ListValue();
  child->Set("titles", titles);
  for (size_t i = 0; i < info->titles.size(); ++i)
    titles->Append(new base::StringValue(info->titles[i]));
}

void AboutMemoryHandler::OnDetailsAvailable() {
  // the root of the JSON hierarchy for about:memory jstemplate
  scoped_ptr<base::DictionaryValue> root(new base::DictionaryValue);
  base::ListValue* browsers = new base::ListValue();
  root->Set("browsers", browsers);

  const std::vector<ProcessData>& browser_processes = processes();

  // Aggregate per-process data into browser summary data.
  base::string16 log_string;
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
    base::DictionaryValue* browser_data = new base::DictionaryValue();
    browsers->Append(browser_data);
    browser_data->SetString("name", browser_processes[index].name);

    BindProcessMetrics(browser_data, &aggregate);

    // We log memory info as we record it.
    if (!log_string.empty())
      log_string += base::ASCIIToUTF16(", ");
    log_string += browser_processes[index].name + base::ASCIIToUTF16(", ") +
                  base::SizeTToString16(aggregate.working_set.priv) +
                  base::ASCIIToUTF16(", ") +
                  base::SizeTToString16(aggregate.working_set.shared) +
                  base::ASCIIToUTF16(", ") +
                  base::SizeTToString16(aggregate.working_set.shareable);
  }
  if (!log_string.empty())
    VLOG(1) << "memory: " << log_string;

  // Set the browser & renderer detailed process data.
  base::DictionaryValue* browser_data = new base::DictionaryValue();
  root->Set("browzr_data", browser_data);
  base::ListValue* child_data = new base::ListValue();
  root->Set("child_data", child_data);

  ProcessData process = browser_processes[0];  // Chrome is the first browser.
  root->SetString("current_browser_name", process.name);

  for (size_t index = 0; index < process.processes.size(); index++) {
    if (process.processes[index].process_type == content::PROCESS_TYPE_BROWSER)
      BindProcessMetrics(browser_data, &process.processes[index]);
    else
      AppendProcess(child_data, &process.processes[index]);
  }

  root->SetBoolean("show_other_browsers",
      browser_defaults::kShowOtherBrowsersInAboutMemory);

  base::DictionaryValue load_time_data;
  load_time_data.SetString(
      "summary_desc",
      l10n_util::GetStringUTF16(IDS_MEMORY_USAGE_SUMMARY_DESC));
  const std::string& app_locale = g_browser_process->GetApplicationLocale();
  webui::SetLoadTimeDataDefaults(app_locale, &load_time_data);
  load_time_data.Set("jstemplateData", root.release());

  std::string data;
  webui::AppendJsonJS(&load_time_data, &data);
  callback_.Run(base::RefCountedString::TakeString(&data));
}

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
    int render_process_id,
    int render_frame_id,
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

    response = ResourceBundle::GetSharedInstance().GetRawDataResource(
        idr).as_string();
#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_CHROMEOS)
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
  } else if (source_name_ == chrome::kChromeUIMemoryHost) {
    response = GetAboutMemoryRedirectResponse(profile());
  } else if (source_name_ == chrome::kChromeUIMemoryRedirectHost) {
    FinishMemoryDataRequest(path, callback);
    return;
#if defined(OS_CHROMEOS)
  } else if (source_name_ == chrome::kChromeUIOSCreditsHost) {
    ChromeOSCreditsHandler::Start(path, callback);
    return;
#endif
#if defined(OS_LINUX) || defined(OS_OPENBSD)
  } else if (source_name_ == chrome::kChromeUISandboxHost) {
    response = AboutSandbox();
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
      path == kStringsJsPath     ||
      path == kMemoryJsPath) {
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

bool AboutUIHTMLSource::ShouldDenyXFrameOptions() const {
#if defined(OS_CHROMEOS)
  if (source_name_ == chrome::kChromeUITermsHost) {
    // chrome://terms page is embedded in iframe to chrome://oobe.
    return false;
  }
#endif
  return content::URLDataSource::ShouldDenyXFrameOptions();
}

AboutUI::AboutUI(content::WebUI* web_ui, const std::string& name)
    : WebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);

#if defined(ENABLE_THEMES)
  // Set up the chrome://theme/ source.
  ThemeSource* theme = new ThemeSource(profile);
  content::URLDataSource::Add(profile, theme);
#endif

  content::URLDataSource::Add(profile, new AboutUIHTMLSource(name, profile));
}
