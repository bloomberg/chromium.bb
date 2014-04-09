// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sstream>

#include "base/command_line.h"
#include "base/files/scoped_temp_dir.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "components/dom_distiller/content/distiller_page_web_contents.h"
#include "components/dom_distiller/core/distiller.h"
#include "components/dom_distiller/core/dom_distiller_database.h"
#include "components/dom_distiller/core/dom_distiller_service.h"
#include "components/dom_distiller/core/dom_distiller_store.h"
#include "components/dom_distiller/core/proto/distilled_article.pb.h"
#include "components/dom_distiller/core/proto/distilled_page.pb.h"
#include "components/dom_distiller/core/task_tracker.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/content_browser_test.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/base/resource/resource_bundle.h"

using content::ContentBrowserTest;

namespace dom_distiller {

namespace {

// The url to distill.
const char* kUrlSwitch = "url";

// Indicates that DNS resolution should be disabled for this test.
const char* kDisableDnsSwitch = "disable-dns";

// Will write the distilled output to the given file instead of to stdout.
const char* kOutputFile = "output-file";

// Indicates to output a serialized protocol buffer instead of human-readable
// output.
const char* kShouldOutputBinary = "output-binary";

scoped_ptr<DomDistillerService> CreateDomDistillerService(
    content::BrowserContext* context,
    const base::FilePath& db_path) {
  scoped_refptr<base::SequencedTaskRunner> background_task_runner =
      content::BrowserThread::GetBlockingPool()->GetSequencedTaskRunner(
          content::BrowserThread::GetBlockingPool()->GetSequenceToken());

  // TODO(cjhopman): use an in-memory database instead of an on-disk one with
  // temporary directory.
  scoped_ptr<DomDistillerDatabase> db(
      new DomDistillerDatabase(background_task_runner));
  scoped_ptr<DomDistillerStore> dom_distiller_store(new DomDistillerStore(
      db.PassAs<DomDistillerDatabaseInterface>(), db_path));

  scoped_ptr<DistillerPageFactory> distiller_page_factory(
      new DistillerPageWebContentsFactory(context));
  scoped_ptr<DistillerURLFetcherFactory> distiller_url_fetcher_factory(
      new DistillerURLFetcherFactory(context->GetRequestContext()));
  scoped_ptr<DistillerFactory> distiller_factory(new DistillerFactoryImpl(
      distiller_page_factory.Pass(), distiller_url_fetcher_factory.Pass()));

  return scoped_ptr<DomDistillerService>(new DomDistillerService(
      dom_distiller_store.PassAs<DomDistillerStoreInterface>(),
      distiller_factory.Pass()));
}

void AddComponentsResources() {
  base::FilePath pak_file;
  base::FilePath pak_dir;
  PathService::Get(base::DIR_MODULE, &pak_dir);
  pak_file = pak_dir.Append(FILE_PATH_LITERAL("components_resources.pak"));
  ui::ResourceBundle::GetSharedInstance().AddDataPackFromPath(
      pak_file, ui::SCALE_FACTOR_NONE);
}

void LogArticle(const DistilledArticleProto& article_proto) {
  std::stringstream output;
  if (CommandLine::ForCurrentProcess()->HasSwitch(kShouldOutputBinary)) {
    output << article_proto.SerializeAsString();
  } else {
    output << "Article Title: " << article_proto.title() << std::endl;
    output << "# of pages: " << article_proto.pages_size() << std::endl;
    for (int i = 0; i < article_proto.pages_size(); ++i) {
      const DistilledPageProto& page = article_proto.pages(i);
      output << "Page " << i << std::endl;
      output << "URL: " << page.url() << std::endl;
      output << "Content: " << page.html() << std::endl;
    }
  }

  std::string data = output.str();
  if (CommandLine::ForCurrentProcess()->HasSwitch(kOutputFile)) {
    base::FilePath filename =
        CommandLine::ForCurrentProcess()->GetSwitchValuePath(kOutputFile);
    base::WriteFile(filename, data.c_str(), data.size());
  } else {
    VLOG(0) << data;
  }
}

}  // namespace

class ContentExtractionRequest : public ViewRequestDelegate {
 public:
  void Start(DomDistillerService* service, base::Closure finished_callback) {
    finished_callback_ = finished_callback;
    viewer_handle_ = service->ViewUrl(this, url_);
  }

  DistilledArticleProto GetArticleCopy() {
    return *article_proto_;
  }

  static scoped_ptr<ContentExtractionRequest> CreateForCommandLine(
      const CommandLine& command_line) {
    GURL url;
    if (command_line.HasSwitch(kUrlSwitch)) {
      std::string url_string = command_line.GetSwitchValueASCII(kUrlSwitch);
      url = GURL(url_string);
    }
    if (!url.is_valid()) {
      ADD_FAILURE() << "No valid url provided";
      return scoped_ptr<ContentExtractionRequest>();
    }
    return scoped_ptr<ContentExtractionRequest>(
        new ContentExtractionRequest(url));
  }

 private:
  ContentExtractionRequest(const GURL& url) : url_(url) {}

  virtual void OnArticleUpdated(ArticleDistillationUpdate article_update)
      OVERRIDE {}

  virtual void OnArticleReady(const DistilledArticleProto* article_proto)
      OVERRIDE {
    article_proto_ = article_proto;
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
    request_ = ContentExtractionRequest::CreateForCommandLine(command_line);
    request_->Start(
        service_.get(),
        base::Bind(&ContentExtractor::Finish, base::Unretained(this)));
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

  void Finish() {
    LogArticle(request_->GetArticleCopy());
    request_.reset();
    service_.reset();
    base::MessageLoop::current()->PostTask(
        FROM_HERE, base::MessageLoop::QuitWhenIdleClosure());
  }

  base::ScopedTempDir db_dir_;
  scoped_ptr<net::ScopedDefaultHostResolverProc> mock_host_resolver_override_;
  scoped_ptr<DomDistillerService> service_;
  scoped_ptr<ContentExtractionRequest> request_;
};

IN_PROC_BROWSER_TEST_F(ContentExtractor, MANUAL_ExtractUrl) {
  Start();
  base::RunLoop().Run();
}

}  // namespace dom_distiller
