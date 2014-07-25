// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/weak_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "components/dom_distiller/content/distiller_page_web_contents.h"
#include "components/dom_distiller/content/web_contents_main_frame_observer.h"
#include "components/dom_distiller/core/distiller_page.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/content_browser_test.h"
#include "content/shell/browser/shell.h"
#include "grit/component_resources.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/resource/resource_bundle.h"

using content::ContentBrowserTest;
using testing::ContainsRegex;
using testing::HasSubstr;
using testing::Not;

namespace dom_distiller {

const char* kSimpleArticlePath = "/simple_article.html";
const char* kVideoArticlePath = "/video_article.html";

class DistillerPageWebContentsTest : public ContentBrowserTest {
 public:
  // ContentBrowserTest:
  virtual void SetUpOnMainThread() OVERRIDE {
    AddComponentsResources();
    SetUpTestServer();
    ContentBrowserTest::SetUpOnMainThread();
  }

  void DistillPage(const base::Closure& quit_closure, const std::string& url) {
    quit_closure_ = quit_closure;
    distiller_page_->DistillPage(
        embedded_test_server()->GetURL(url),
        dom_distiller::proto::DomDistillerOptions(),
        base::Bind(&DistillerPageWebContentsTest::OnPageDistillationFinished,
                   this));
  }

  void OnPageDistillationFinished(scoped_ptr<DistilledPageInfo> distilled_page,
                                  bool distillation_successful) {
    page_info_ = distilled_page.Pass();
    quit_closure_.Run();
  }

 private:
  void AddComponentsResources() {
    base::FilePath pak_file;
    base::FilePath pak_dir;
    PathService::Get(base::DIR_MODULE, &pak_dir);
    pak_file = pak_dir.Append(FILE_PATH_LITERAL("components_resources.pak"));
    ui::ResourceBundle::GetSharedInstance().AddDataPackFromPath(
        pak_file, ui::SCALE_FACTOR_NONE);
  }

  void SetUpTestServer() {
    base::FilePath path;
    PathService::Get(base::DIR_SOURCE_ROOT, &path);
    path = path.AppendASCII("components/test/data/dom_distiller");
    embedded_test_server()->ServeFilesFromDirectory(path);
    ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());
  }

 protected:
  void RunUseCurrentWebContentsTest(const std::string& url,
                                    bool expect_new_web_contents,
                                    bool setup_main_frame_observer,
                                    bool wait_for_document_loaded);

  DistillerPageWebContents* distiller_page_;
  base::Closure quit_closure_;
  scoped_ptr<DistilledPageInfo> page_info_;
};

// Use this class to be able to leak the WebContents, which is needed for when
// the current WebContents is used for distillation.
class TestDistillerPageWebContents : public DistillerPageWebContents {
 public:
  TestDistillerPageWebContents(
      content::BrowserContext* browser_context,
      const gfx::Size& render_view_size,
      scoped_ptr<SourcePageHandleWebContents> optional_web_contents_handle,
      bool expect_new_web_contents)
      : DistillerPageWebContents(browser_context, render_view_size,
                                 optional_web_contents_handle.Pass()),
        expect_new_web_contents_(expect_new_web_contents),
        new_web_contents_created_(false) {}

  virtual void CreateNewWebContents(const GURL& url) OVERRIDE {
    ASSERT_EQ(true, expect_new_web_contents_);
    new_web_contents_created_ = true;
    // DistillerPageWebContents::CreateNewWebContents resets the scoped_ptr to
    // the WebContents, so intentionally leak WebContents here, since it is
    // owned by the shell.
    content::WebContents* web_contents = web_contents_.release();
    web_contents->GetLastCommittedURL();
    DistillerPageWebContents::CreateNewWebContents(url);
  }

  virtual ~TestDistillerPageWebContents() {
    if (!expect_new_web_contents_) {
      // Intentionally leaking WebContents, since it is owned by the shell.
      content::WebContents* web_contents = web_contents_.release();
      web_contents->GetLastCommittedURL();
    }
  }

  bool new_web_contents_created() { return new_web_contents_created_; }

 private:
  bool expect_new_web_contents_;
  bool new_web_contents_created_;
};

// Helper class to know how far in the loading process the current WebContents
// has come. It will call the callback either after
// DidCommitProvisionalLoadForFrame or DocumentLoadedInFrame is called for the
// main frame, based on the value of |wait_for_document_loaded|.
class WebContentsMainFrameHelper : public content::WebContentsObserver {
 public:
  WebContentsMainFrameHelper(content::WebContents* web_contents,
                             const base::Closure& callback,
                             bool wait_for_document_loaded)
      : WebContentsObserver(web_contents),
        callback_(callback),
        wait_for_document_loaded_(wait_for_document_loaded) {}

  virtual void DidCommitProvisionalLoadForFrame(
      content::RenderFrameHost* render_frame_host,
      const GURL& url,
      content::PageTransition transition_type) OVERRIDE {
    if (wait_for_document_loaded_)
      return;
    if (!render_frame_host->GetParent())
      callback_.Run();
  }

  virtual void DocumentLoadedInFrame(
      content::RenderFrameHost* render_frame_host) OVERRIDE {
    if (wait_for_document_loaded_) {
      if (!render_frame_host->GetParent())
        callback_.Run();
    }
  }

 private:
  base::Closure callback_;
  bool wait_for_document_loaded_;
};

IN_PROC_BROWSER_TEST_F(DistillerPageWebContentsTest, BasicDistillationWorks) {
  DistillerPageWebContents distiller_page(
      shell()->web_contents()->GetBrowserContext(),
      shell()->web_contents()->GetContainerBounds().size(),
      scoped_ptr<SourcePageHandleWebContents>());
  distiller_page_ = &distiller_page;

  base::RunLoop run_loop;
  DistillPage(run_loop.QuitClosure(), kSimpleArticlePath);
  run_loop.Run();

  EXPECT_EQ("Test Page Title", page_info_.get()->title);
  EXPECT_THAT(page_info_.get()->html, HasSubstr("Lorem ipsum"));
  EXPECT_THAT(page_info_.get()->html, Not(HasSubstr("questionable content")));
  EXPECT_EQ("", page_info_.get()->next_page_url);
  EXPECT_EQ("", page_info_.get()->prev_page_url);
}

IN_PROC_BROWSER_TEST_F(DistillerPageWebContentsTest, HandlesRelativeLinks) {
  DistillerPageWebContents distiller_page(
      shell()->web_contents()->GetBrowserContext(),
      shell()->web_contents()->GetContainerBounds().size(),
      scoped_ptr<SourcePageHandleWebContents>());
  distiller_page_ = &distiller_page;

  base::RunLoop run_loop;
  DistillPage(run_loop.QuitClosure(), kSimpleArticlePath);
  run_loop.Run();

  // A relative link should've been updated.
  EXPECT_THAT(page_info_.get()->html,
              ContainsRegex("href=\"http://127.0.0.1:.*/relativelink.html\""));
  EXPECT_THAT(page_info_.get()->html,
              HasSubstr("href=\"http://www.google.com/absolutelink.html\""));
}

IN_PROC_BROWSER_TEST_F(DistillerPageWebContentsTest, HandlesRelativeImages) {
  DistillerPageWebContents distiller_page(
      shell()->web_contents()->GetBrowserContext(),
      shell()->web_contents()->GetContainerBounds().size(),
      scoped_ptr<SourcePageHandleWebContents>());
  distiller_page_ = &distiller_page;

  base::RunLoop run_loop;
  DistillPage(run_loop.QuitClosure(), kSimpleArticlePath);
  run_loop.Run();

  // A relative link should've been updated.
  EXPECT_THAT(page_info_.get()->html,
              ContainsRegex("src=\"http://127.0.0.1:.*/relativeimage.png\""));
  EXPECT_THAT(page_info_.get()->html,
              HasSubstr("src=\"http://www.google.com/absoluteimage.png\""));
}


IN_PROC_BROWSER_TEST_F(DistillerPageWebContentsTest, HandlesRelativeVideos) {
  DistillerPageWebContents distiller_page(
      shell()->web_contents()->GetBrowserContext(),
      shell()->web_contents()->GetContainerBounds().size(),
      scoped_ptr<SourcePageHandleWebContents>());
  distiller_page_ = &distiller_page;

  base::RunLoop run_loop;
  DistillPage(run_loop.QuitClosure(), kVideoArticlePath);
  run_loop.Run();

  // A relative source/track should've been updated.
  EXPECT_THAT(
      page_info_.get()->html,
      ContainsRegex("src=\"http://127.0.0.1:.*/relative_video.mp4\""));
  EXPECT_THAT(
      page_info_.get()->html,
      ContainsRegex("src=\"http://127.0.0.1:.*/relative_track_en.vtt\""));
  EXPECT_THAT(
      page_info_.get()->html,
      HasSubstr("src=\"http://www.google.com/absolute_video.ogg\""));
  EXPECT_THAT(
      page_info_.get()->html,
      HasSubstr("src=\"http://www.google.com/absolute_track_fr.vtt\""));
}

IN_PROC_BROWSER_TEST_F(DistillerPageWebContentsTest, VisibilityDetection) {
  DistillerPageWebContents distiller_page(
      shell()->web_contents()->GetBrowserContext(),
      shell()->web_contents()->GetContainerBounds().size(),
      scoped_ptr<SourcePageHandleWebContents>());
  distiller_page_ = &distiller_page;

  // visble_style.html and invisible_style.html only differ by the visibility
  // internal stylesheet.

  {
    base::RunLoop run_loop;
    DistillPage(run_loop.QuitClosure(), "/visible_style.html");
    run_loop.Run();
    EXPECT_THAT(page_info_.get()->html, HasSubstr("Lorem ipsum"));
  }

  {
    base::RunLoop run_loop;
    DistillPage(run_loop.QuitClosure(), "/invisible_style.html");
    run_loop.Run();
    EXPECT_THAT(page_info_.get()->html, Not(HasSubstr("Lorem ipsum")));
  }
}

IN_PROC_BROWSER_TEST_F(DistillerPageWebContentsTest,
                       UsingCurrentWebContentsWrongUrl) {
  std::string url("/bogus");
  bool expect_new_web_contents = true;
  bool setup_main_frame_observer = true;
  bool wait_for_document_loaded = true;
  RunUseCurrentWebContentsTest(url,
                               expect_new_web_contents,
                               setup_main_frame_observer,
                               wait_for_document_loaded);
}

IN_PROC_BROWSER_TEST_F(DistillerPageWebContentsTest,
                       UsingCurrentWebContentsNoMainFrameObserver) {
  std::string url(kSimpleArticlePath);
  bool expect_new_web_contents = true;
  bool setup_main_frame_observer = false;
  bool wait_for_document_loaded = true;
  RunUseCurrentWebContentsTest(url,
                               expect_new_web_contents,
                               setup_main_frame_observer,
                               wait_for_document_loaded);
}

IN_PROC_BROWSER_TEST_F(DistillerPageWebContentsTest,
                       UsingCurrentWebContentsNotFinishedLoadingYet) {
  std::string url(kSimpleArticlePath);
  bool expect_new_web_contents = false;
  bool setup_main_frame_observer = true;
  bool wait_for_document_loaded = false;
  RunUseCurrentWebContentsTest(url,
                               expect_new_web_contents,
                               setup_main_frame_observer,
                               wait_for_document_loaded);
}

IN_PROC_BROWSER_TEST_F(DistillerPageWebContentsTest,
                       UsingCurrentWebContentsReadyForDistillation) {
  std::string url(kSimpleArticlePath);
  bool expect_new_web_contents = false;
  bool setup_main_frame_observer = true;
  bool wait_for_document_loaded = true;
  RunUseCurrentWebContentsTest(url,
                               expect_new_web_contents,
                               setup_main_frame_observer,
                               wait_for_document_loaded);
}

void DistillerPageWebContentsTest::RunUseCurrentWebContentsTest(
    const std::string& url,
    bool expect_new_web_contents,
    bool setup_main_frame_observer,
    bool wait_for_document_loaded) {
  content::WebContents* current_web_contents = shell()->web_contents();
  if (setup_main_frame_observer) {
    dom_distiller::WebContentsMainFrameObserver::CreateForWebContents(
        current_web_contents);
  }
  base::RunLoop url_loaded_runner;
  WebContentsMainFrameHelper main_frame_loaded(current_web_contents,
                                               url_loaded_runner.QuitClosure(),
                                               wait_for_document_loaded);
  current_web_contents->GetController().LoadURL(
      embedded_test_server()->GetURL(url),
      content::Referrer(),
      content::PAGE_TRANSITION_TYPED,
      std::string());
  url_loaded_runner.Run();

  scoped_ptr<content::WebContents> old_web_contents_sptr(current_web_contents);
  scoped_ptr<SourcePageHandleWebContents> source_page_handle(
      new SourcePageHandleWebContents(old_web_contents_sptr.Pass()));

  TestDistillerPageWebContents distiller_page(
      shell()->web_contents()->GetBrowserContext(),
      shell()->web_contents()->GetContainerBounds().size(),
      source_page_handle.Pass(),
      expect_new_web_contents);
  distiller_page_ = &distiller_page;

  base::RunLoop run_loop;
  DistillPage(run_loop.QuitClosure(), kSimpleArticlePath);
  run_loop.Run();

  // Sanity check of distillation process.
  EXPECT_EQ(expect_new_web_contents, distiller_page.new_web_contents_created());
  EXPECT_EQ("Test Page Title", page_info_.get()->title);
}

IN_PROC_BROWSER_TEST_F(DistillerPageWebContentsTest, MarkupInfo) {
  DistillerPageWebContents distiller_page(
      shell()->web_contents()->GetBrowserContext(),
      shell()->web_contents()->GetContainerBounds().size(),
      scoped_ptr<SourcePageHandleWebContents>());
  distiller_page_ = &distiller_page;

  base::RunLoop run_loop;
  DistillPage(run_loop.QuitClosure(), "/markup_article.html");
  run_loop.Run();

  EXPECT_THAT(page_info_.get()->html, HasSubstr("Lorem ipsum"));
  EXPECT_EQ("Marked-up Markup Test Page Title", page_info_.get()->title);

  const DistilledPageInfo::MarkupInfo& markup_info = page_info_->markup_info;
  EXPECT_EQ("Marked-up Markup Test Page Title", markup_info.title);
  EXPECT_EQ("Article", markup_info.type);
  EXPECT_EQ("http://test/markup.html", markup_info.url);
  EXPECT_EQ("This page tests Markup Info.", markup_info.description);
  EXPECT_EQ("Whoever Published", markup_info.publisher);
  EXPECT_EQ("Copyright 2000-2014 Whoever Copyrighted", markup_info.copyright);
  EXPECT_EQ("Whoever Authored", markup_info.author);

  const DistilledPageInfo::MarkupArticle& markup_article = markup_info.article;
  EXPECT_EQ("Whatever Section", markup_article.section);
  EXPECT_EQ("July 23, 2014", markup_article.published_time);
  EXPECT_EQ("2014-07-23T23:59", markup_article.modified_time);
  EXPECT_EQ("", markup_article.expiration_time);
  ASSERT_EQ(1U, markup_article.authors.size());
  EXPECT_EQ("Whoever Authored", markup_article.authors[0]);

  ASSERT_EQ(2U, markup_info.images.size());

  const DistilledPageInfo::MarkupImage& markup_image1 = markup_info.images[0];
  EXPECT_EQ("http://test/markup1.jpeg", markup_image1.url);
  EXPECT_EQ("https://test/markup1.jpeg", markup_image1.secure_url);
  EXPECT_EQ("jpeg", markup_image1.type);
  EXPECT_EQ("", markup_image1.caption);
  EXPECT_EQ(600, markup_image1.width);
  EXPECT_EQ(400, markup_image1.height);

  const DistilledPageInfo::MarkupImage& markup_image2 = markup_info.images[1];
  EXPECT_EQ("http://test/markup2.gif", markup_image2.url);
  EXPECT_EQ("https://test/markup2.gif", markup_image2.secure_url);
  EXPECT_EQ("gif", markup_image2.type);
  EXPECT_EQ("", markup_image2.caption);
  EXPECT_EQ(1000, markup_image2.width);
  EXPECT_EQ(600, markup_image2.height);
}

}  // namespace dom_distiller
