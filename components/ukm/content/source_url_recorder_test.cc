// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ukm/content/source_url_recorder.h"
#include "components/ukm/test_ukm_recorder.h"
#include "components/ukm/ukm_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using content::NavigationSimulator;

class SourceUrlRecorderWebContentsObserverTest
    : public content::RenderViewHostTestHarness {
 public:
  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    ukm::InitializeSourceUrlRecorderForWebContents(web_contents());
  }

 protected:
  ukm::TestAutoSetUkmRecorder test_ukm_recorder_;
};

TEST_F(SourceUrlRecorderWebContentsObserverTest, Basic) {
  GURL url("https://www.example.com/");
  NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(), url);
  EXPECT_EQ(1ul, test_ukm_recorder_.sources_count());
  const ukm::UkmSource* ukm_source = test_ukm_recorder_.GetSourceForUrl(url);
  ASSERT_NE(nullptr, ukm_source);
  EXPECT_EQ(url, ukm_source->url());
  EXPECT_TRUE(ukm_source->initial_url().is_empty());
}

TEST_F(SourceUrlRecorderWebContentsObserverTest, InitialUrl) {
  GURL initial_url("https://www.a.com/");
  GURL final_url("https://www.b.com/");
  auto simulator =
      NavigationSimulator::CreateRendererInitiated(initial_url, main_rfh());
  simulator->Start();
  simulator->Redirect(final_url);
  simulator->Commit();
  EXPECT_EQ(1ul, test_ukm_recorder_.sources_count());
  const ukm::UkmSource* ukm_source =
      test_ukm_recorder_.GetSourceForUrl(final_url);
  ASSERT_NE(nullptr, ukm_source);
  EXPECT_EQ(final_url, ukm_source->url());
  EXPECT_EQ(initial_url, ukm_source->initial_url());
}

TEST_F(SourceUrlRecorderWebContentsObserverTest, IgnoreUnsupportedScheme) {
  NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                    GURL("about:blank"));
  EXPECT_EQ(0ul, test_ukm_recorder_.sources_count());
}

TEST_F(SourceUrlRecorderWebContentsObserverTest, IgnoreUrlInSubframe) {
  GURL main_frame_url("https://www.example.com/");
  GURL sub_frame_url("https://www.example.com/iframe.html");
  NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                    main_frame_url);
  NavigationSimulator::NavigateAndCommitFromDocument(
      sub_frame_url,
      content::RenderFrameHostTester::For(main_rfh())->AppendChild("subframe"));
  EXPECT_EQ(1ul, test_ukm_recorder_.sources_count());
  EXPECT_NE(nullptr, test_ukm_recorder_.GetSourceForUrl(main_frame_url));
  EXPECT_EQ(nullptr, test_ukm_recorder_.GetSourceForUrl(sub_frame_url));
}

TEST_F(SourceUrlRecorderWebContentsObserverTest, IgnoreSameDocumentNavigation) {
  GURL url("https://www.example.com/");
  GURL same_document_url("https://www.example.com/#samedocument");
  NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(), url);
  NavigationSimulator::CreateRendererInitiated(same_document_url, main_rfh())
      ->CommitSameDocument();
  EXPECT_EQ(same_document_url, web_contents()->GetLastCommittedURL());
  EXPECT_EQ(1ul, test_ukm_recorder_.sources_count());
  EXPECT_EQ(nullptr, test_ukm_recorder_.GetSourceForUrl(same_document_url));
  const ukm::UkmSource* ukm_source = test_ukm_recorder_.GetSourceForUrl(url);
  ASSERT_NE(nullptr, ukm_source);
  EXPECT_EQ(url, ukm_source->url());
  EXPECT_TRUE(ukm_source->initial_url().is_empty());
}
