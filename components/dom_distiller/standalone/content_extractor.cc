// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sstream>

#include "base/command_line.h"
#include "base/files/scoped_temp_dir.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "components/dom_distiller/content/distiller_page_web_contents.h"
#include "components/dom_distiller/core/article_entry.h"
#include "components/dom_distiller/core/distilled_page_prefs.h"
#include "components/dom_distiller/core/distiller.h"
#include "components/dom_distiller/core/dom_distiller_service.h"
#include "components/dom_distiller/core/dom_distiller_store.h"
#include "components/dom_distiller/core/proto/distilled_article.pb.h"
#include "components/dom_distiller/core/proto/distilled_page.pb.h"
#include "components/dom_distiller/core/task_tracker.h"
#include "components/leveldb_proto/proto_database.h"
#include "components/leveldb_proto/proto_database_impl.h"
#include "components/pref_registry/testing_pref_service_syncable.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/content_browser_test.h"
#include "content/shell/browser/shell.h"
#include "google/protobuf/io/coded_stream.h"
#include "google/protobuf/io/zero_copy_stream_impl_lite.h"
#include "net/dns/mock_host_resolver.h"
#include "third_party/dom_distiller_js/dom_distiller.pb.h"
#include "ui/base/resource/resource_bundle.h"

using content::ContentBrowserTest;

namespace dom_distiller {

namespace {

// The url to distill.
const char* kUrlSwitch = "url";

// A space-separated list of urls to distill.
const char* kUrlsSwitch = "urls";

// Indicates that DNS resolution should be disabled for this test.
const char* kDisableDnsSwitch = "disable-dns";

// Will write the distilled output to the given file instead of to stdout.
const char* kOutputFile = "output-file";

// Indicates to output a serialized protocol buffer instead of human-readable
// output.
const char* kShouldOutputBinary = "output-binary";

// Indicates to output only the text of the article and not the enclosing html.
const char* kExtractTextOnly = "extract-text-only";

// Indicates to include debug output.
const char* kDebugLevel = "debug-level";

// Maximum number of concurrent started extractor requests.
const int kMaxExtractorTasks = 8;

scoped_ptr<DomDistillerService> CreateDomDistillerService(
    content::BrowserContext* context,
    const base::FilePath& db_path) {
  scoped_refptr<base::SequencedTaskRunner> background_task_runner =
      content::BrowserThread::GetBlockingPool()->GetSequencedTaskRunner(
          content::BrowserThread::GetBlockingPool()->GetSequenceToken());

  // TODO(cjhopman): use an in-memory database instead of an on-disk one with
  // temporary directory.
  scoped_ptr<leveldb_proto::ProtoDatabaseImpl<ArticleEntry> > db(
      new leveldb_proto::ProtoDatabaseImpl<ArticleEntry>(
          background_task_runner));
  scoped_ptr<DomDistillerStore> dom_distiller_store(new DomDistillerStore(
      db.PassAs<leveldb_proto::ProtoDatabase<ArticleEntry> >(), db_path));

  scoped_ptr<DistillerPageFactory> distiller_page_factory(
      new DistillerPageWebContentsFactory(context));
  scoped_ptr<DistillerURLFetcherFactory> distiller_url_fetcher_factory(
      new DistillerURLFetcherFactory(context->GetRequestContext()));

  dom_distiller::proto::DomDistillerOptions options;
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(kExtractTextOnly)) {
    options.set_extract_text_only(true);
  }
  int debug_level = 0;
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(kDebugLevel) &&
      base::StringToInt(
          base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
              kDebugLevel),
          &debug_level)) {
    options.set_debug_level(debug_level);
  }
  scoped_ptr<DistillerFactory> distiller_factory(
      new DistillerFactoryImpl(distiller_url_fetcher_factory.Pass(), options));

  // Setting up PrefService for DistilledPagePrefs.
  user_prefs::TestingPrefServiceSyncable* pref_service =
      new user_prefs::TestingPrefServiceSyncable();
  DistilledPagePrefs::RegisterProfilePrefs(pref_service->registry());

  return scoped_ptr<DomDistillerService>(new DomDistillerService(
      dom_distiller_store.PassAs<DomDistillerStoreInterface>(),
      distiller_factory.Pass(),
      distiller_page_factory.Pass(),
      scoped_ptr<DistilledPagePrefs>(
          new DistilledPagePrefs(pref_service))));
}

void AddComponentsResources() {
  base::FilePath pak_file;
  base::FilePath pak_dir;
  PathService::Get(base::DIR_MODULE, &pak_dir);
  pak_file = pak_dir.Append(FILE_PATH_LITERAL("components_resources.pak"));
  ui::ResourceBundle::GetSharedInstance().AddDataPackFromPath(
      pak_file, ui::SCALE_FACTOR_NONE);
}

bool WriteProtobufWithSize(
    const google::protobuf::MessageLite& message,
    google::protobuf::io::ZeroCopyOutputStream* output_stream) {
  google::protobuf::io::CodedOutputStream coded_output(output_stream);

  // Write the size.
  const int size = message.ByteSize();
  coded_output.WriteLittleEndian32(size);
  message.SerializeWithCachedSizes(&coded_output);
  return !coded_output.HadError();
}

std::string GetReadableArticleString(
    const DistilledArticleProto& article_proto) {
  std::stringstream output;
  output << "Article Title: " << article_proto.title() << std::endl;
  output << "# of pages: " << article_proto.pages_size() << std::endl;
  for (int i = 0; i < article_proto.pages_size(); ++i) {
    const DistilledPageProto& page = article_proto.pages(i);
    output << "Page " << i << std::endl;
    output << "URL: " << page.url() << std::endl;
    output << "Content: " << page.html() << std::endl;
    if (page.has_debug_info() && page.debug_info().has_log())
      output << "Log: " << page.debug_info().log() << std::endl;
  }
  return output.str();
}

}  // namespace

class ContentExtractionRequest : public ViewRequestDelegate {
 public:
  void Start(DomDistillerService* service, const gfx::Size& render_view_size,
             base::Closure finished_callback) {
    finished_callback_ = finished_callback;
    viewer_handle_ =
        service->ViewUrl(this,
                         service->CreateDefaultDistillerPage(render_view_size),
                         url_);
  }

  DistilledArticleProto GetArticleCopy() {
    return *article_proto_;
  }

  static ScopedVector<ContentExtractionRequest> CreateForCommandLine(
      const CommandLine& command_line) {
    ScopedVector<ContentExtractionRequest> requests;
    if (command_line.HasSwitch(kUrlSwitch)) {
      GURL url;
      std::string url_string = command_line.GetSwitchValueASCII(kUrlSwitch);
      url = GURL(url_string);
      if (url.is_valid()) {
        requests.push_back(new ContentExtractionRequest(url));
      }
    } else if (command_line.HasSwitch(kUrlsSwitch)) {
      std::string urls_string = command_line.GetSwitchValueASCII(kUrlsSwitch);
      std::vector<std::string> urls;
      base::SplitString(urls_string, ' ', &urls);
      for (size_t i = 0; i < urls.size(); ++i) {
        GURL url(urls[i]);
        if (url.is_valid()) {
          requests.push_back(new ContentExtractionRequest(url));
        } else {
          ADD_FAILURE() << "Bad url";
        }
      }
    }
    if (requests.empty()) {
      ADD_FAILURE() << "No valid url provided";
    }

    return requests.Pass();
  }

 private:
  ContentExtractionRequest(const GURL& url) : url_(url) {}

  virtual void OnArticleUpdated(ArticleDistillationUpdate article_update)
      OVERRIDE {}

  virtual void OnArticleReady(const DistilledArticleProto* article_proto)
      OVERRIDE {
    article_proto_ = article_proto;
    CHECK(article_proto->pages_size()) << "Failed extracting " << url_;
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        finished_callback_);
  }

  const DistilledArticleProto* article_proto_;
  scoped_ptr<ViewerHandle> viewer_handle_;
  GURL url_;
  base::Closure finished_callback_;
};

class ContentExtractor : public ContentBrowserTest {
 public:
  ContentExtractor()
      : pending_tasks_(0),
        max_tasks_(kMaxExtractorTasks),
        next_request_(0),
        output_data_(),
        protobuf_output_stream_(
            new google::protobuf::io::StringOutputStream(&output_data_)) {}

  // Change behavior of the default host resolver to avoid DNS lookup errors, so
  // we can make network calls.
  virtual void SetUpOnMainThread() OVERRIDE {
    if (!CommandLine::ForCurrentProcess()->HasSwitch(kDisableDnsSwitch)) {
      EnableDNSLookupForThisTest();
    }
    CHECK(db_dir_.CreateUniqueTempDir());
    AddComponentsResources();
  }

  virtual void TearDownOnMainThread() OVERRIDE {
    DisableDNSLookupForThisTest();
  }

 protected:
  // Creates the DomDistillerService and creates and starts the extraction
  // request.
  void Start() {
    content::BrowserContext* context =
        shell()->web_contents()->GetBrowserContext();
    service_ = CreateDomDistillerService(context,
                                         db_dir_.path());
    const CommandLine& command_line = *CommandLine::ForCurrentProcess();
    requests_ = ContentExtractionRequest::CreateForCommandLine(command_line);
    PumpQueue();
  }

  void PumpQueue() {
    while (pending_tasks_ < max_tasks_ && next_request_ < requests_.size()) {
      requests_[next_request_]->Start(
          service_.get(),
          shell()->web_contents()->GetContainerBounds().size(),
          base::Bind(&ContentExtractor::FinishRequest, base::Unretained(this)));
      ++next_request_;
      ++pending_tasks_;
    }
  }

 private:
  // Change behavior of the default host resolver to allow DNS lookup
  // to proceed instead of being blocked by the test infrastructure.
  void EnableDNSLookupForThisTest() {
    // mock_host_resolver_override_ takes ownership of the resolver.
    scoped_refptr<net::RuleBasedHostResolverProc> resolver =
        new net::RuleBasedHostResolverProc(host_resolver());
    resolver->AllowDirectLookup("*");
    mock_host_resolver_override_.reset(
        new net::ScopedDefaultHostResolverProc(resolver.get()));
  }

  // We need to reset the DNS lookup when we finish, or the test will fail.
  void DisableDNSLookupForThisTest() {
    mock_host_resolver_override_.reset();
  }

  void FinishRequest() {
    --pending_tasks_;
    if (next_request_ == requests_.size() && pending_tasks_ == 0) {
      Finish();
    } else {
      PumpQueue();
    }
  }

  void DoArticleOutput() {
    for (size_t i = 0; i < requests_.size(); ++i) {
      const DistilledArticleProto& article = requests_[i]->GetArticleCopy();
      if (CommandLine::ForCurrentProcess()->HasSwitch(kShouldOutputBinary)) {
        WriteProtobufWithSize(article, protobuf_output_stream_.get());
      } else {
        output_data_ += GetReadableArticleString(article) + "\n";
      }
    }

    if (CommandLine::ForCurrentProcess()->HasSwitch(kOutputFile)) {
      base::FilePath filename =
          CommandLine::ForCurrentProcess()->GetSwitchValuePath(kOutputFile);
      ASSERT_EQ(
          (int)output_data_.size(),
          base::WriteFile(filename, output_data_.c_str(), output_data_.size()));
    } else {
      VLOG(0) << output_data_;
    }
  }

  void Finish() {
    DoArticleOutput();
    requests_.clear();
    service_.reset();
    base::MessageLoop::current()->PostTask(
        FROM_HERE, base::MessageLoop::QuitWhenIdleClosure());
  }

  size_t pending_tasks_;
  size_t max_tasks_;
  size_t next_request_;

  base::ScopedTempDir db_dir_;
  scoped_ptr<net::ScopedDefaultHostResolverProc> mock_host_resolver_override_;
  scoped_ptr<DomDistillerService> service_;
  ScopedVector<ContentExtractionRequest> requests_;

  std::string output_data_;
  scoped_ptr<google::protobuf::io::StringOutputStream> protobuf_output_stream_;
};

IN_PROC_BROWSER_TEST_F(ContentExtractor, MANUAL_ExtractUrl) {
  Start();
  base::RunLoop().Run();
}

}  // namespace dom_distiller
