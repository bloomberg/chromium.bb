// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/test/histogram_tester.h"
#include "chrome/browser/external_protocol/external_protocol_handler.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/external_protocol_dialog_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/external_protocol_dialog.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/controls/message_box_view.h"
#include "url/gurl.h"

namespace test {

class ExternalProtocolDialogTestApi {
 public:
  explicit ExternalProtocolDialogTestApi(ExternalProtocolDialog* dialog)
      : dialog_(dialog) {}

  void SetCheckBoxSelected(bool checked) {
    dialog_->message_box_view_->SetCheckBoxSelected(checked);
  }

 private:
  ExternalProtocolDialog* dialog_;

  DISALLOW_COPY_AND_ASSIGN(ExternalProtocolDialogTestApi);
};

}  // namespace test

// Wrapper dialog delegate that sets |called|, |accept|, |cancel|, and
// |dont_block| bools based on what is called by the ExternalProtocolDialog.
class TestExternalProtocolDialogDelegate
    : public ExternalProtocolDialogDelegate {
 public:
  TestExternalProtocolDialogDelegate(const GURL& url,
                                     int render_process_host_id,
                                     int routing_id,
                                     bool* called,
                                     bool* accept,
                                     bool* cancel,
                                     bool* dont_block)
      : ExternalProtocolDialogDelegate(url, render_process_host_id, routing_id),
        called_(called),
        accept_(accept),
        cancel_(cancel),
        dont_block_(dont_block) {}

  // ExternalProtocolDialogDelegate:
  void DoAccept(const GURL& url, bool dont_block) const override {
    // Don't call the base impl because it will actually launch |url|.
    *called_ = true;
    *accept_ = true;
    *cancel_ = false;
    *dont_block_ = dont_block;
  }

  void DoCancel(const GURL& url, bool dont_block) const override {
    // Don't call the base impl because it will actually launch |url|.
    *called_ = true;
    *accept_ = false;
    *cancel_ = true;
    *dont_block_ = dont_block;
  }

 private:
  bool* called_;
  bool* accept_;
  bool* cancel_;
  bool* dont_block_;

  DISALLOW_COPY_AND_ASSIGN(TestExternalProtocolDialogDelegate);
};

class ExternalProtocolDialogBrowserTest : public InProcessBrowserTest {
 public:
  ExternalProtocolDialogBrowserTest() {}

  void ShowDialog() {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    int render_process_host_id = web_contents->GetRenderProcessHost()->GetID();
    int routing_id = web_contents->GetRenderViewHost()->GetRoutingID();
    dialog_ = new ExternalProtocolDialog(
        base::MakeUnique<TestExternalProtocolDialogDelegate>(
            GURL("telnet://12345"), render_process_host_id, routing_id,
            &called_, &accept_, &cancel_, &dont_block_),
        render_process_host_id, routing_id);
  }

  void SetChecked(bool checked) {
    test::ExternalProtocolDialogTestApi(dialog_).SetCheckBoxSelected(checked);
  }

  base::HistogramTester histogram_tester_;

 protected:
  ExternalProtocolDialog* dialog_ = nullptr;
  bool called_ = false;
  bool accept_ = false;
  bool cancel_ = false;
  bool dont_block_ = false;

 private:
  DISALLOW_COPY_AND_ASSIGN(ExternalProtocolDialogBrowserTest);
};

IN_PROC_BROWSER_TEST_F(ExternalProtocolDialogBrowserTest, TestAccept) {
  ShowDialog();
  EXPECT_TRUE(dialog_->Accept());
  EXPECT_TRUE(called_);
  EXPECT_TRUE(accept_);
  EXPECT_FALSE(cancel_);
  EXPECT_FALSE(dont_block_);
  histogram_tester_.ExpectBucketCount(
      ExternalProtocolHandler::kRememberCheckboxMetric, 0 /* false */, 1);
  histogram_tester_.ExpectBucketCount(
      ExternalProtocolHandler::kHandleStateMetric,
      ExternalProtocolHandler::LAUNCH, 1);
}

IN_PROC_BROWSER_TEST_F(ExternalProtocolDialogBrowserTest,
                       TestAcceptWithChecked) {
  ShowDialog();
  SetChecked(true);
  EXPECT_TRUE(dialog_->Accept());
  EXPECT_TRUE(called_);
  EXPECT_TRUE(accept_);
  EXPECT_FALSE(cancel_);
  EXPECT_TRUE(dont_block_);
  histogram_tester_.ExpectBucketCount(
      ExternalProtocolHandler::kRememberCheckboxMetric, 1 /* true */, 1);
  histogram_tester_.ExpectBucketCount(
      ExternalProtocolHandler::kHandleStateMetric,
      ExternalProtocolHandler::CHECKED_LAUNCH, 1);
}

IN_PROC_BROWSER_TEST_F(ExternalProtocolDialogBrowserTest, TestCancel) {
  ShowDialog();
  EXPECT_TRUE(dialog_->Cancel());
  EXPECT_TRUE(called_);
  EXPECT_FALSE(accept_);
  EXPECT_TRUE(cancel_);
  EXPECT_FALSE(dont_block_);
  histogram_tester_.ExpectBucketCount(
      ExternalProtocolHandler::kRememberCheckboxMetric, 0 /* false */, 1);
  histogram_tester_.ExpectBucketCount(
      ExternalProtocolHandler::kHandleStateMetric,
      ExternalProtocolHandler::DONT_LAUNCH, 1);
}

IN_PROC_BROWSER_TEST_F(ExternalProtocolDialogBrowserTest,
                       TestCancelWithChecked) {
  ShowDialog();
  SetChecked(true);
  EXPECT_TRUE(dialog_->Cancel());
  EXPECT_TRUE(called_);
  EXPECT_FALSE(accept_);
  EXPECT_TRUE(cancel_);
  EXPECT_TRUE(dont_block_);
  histogram_tester_.ExpectBucketCount(
      ExternalProtocolHandler::kRememberCheckboxMetric, 1 /* true */, 1);
  histogram_tester_.ExpectBucketCount(
      ExternalProtocolHandler::kHandleStateMetric,
      ExternalProtocolHandler::CHECKED_DONT_LAUNCH, 1);
}

IN_PROC_BROWSER_TEST_F(ExternalProtocolDialogBrowserTest, TestClose) {
  // Closing the dialog should always call DoCancel() with |dont_block| = false.
  ShowDialog();
  EXPECT_TRUE(dialog_->Close());
  EXPECT_TRUE(called_);
  EXPECT_FALSE(accept_);
  EXPECT_TRUE(cancel_);
  EXPECT_FALSE(dont_block_);
  // No histogram data
  histogram_tester_.ExpectTotalCount(
      ExternalProtocolHandler::kRememberCheckboxMetric, 0);
  histogram_tester_.ExpectTotalCount(
      ExternalProtocolHandler::kHandleStateMetric, 0);
}

IN_PROC_BROWSER_TEST_F(ExternalProtocolDialogBrowserTest,
                       TestCloseWithChecked) {
  // Closing the dialog should always call DoCancel() with |dont_block| = false.
  ShowDialog();
  SetChecked(true);
  EXPECT_TRUE(dialog_->Close());
  EXPECT_TRUE(called_);
  EXPECT_FALSE(accept_);
  EXPECT_TRUE(cancel_);
  EXPECT_FALSE(dont_block_);
  // No histogram data
  histogram_tester_.ExpectTotalCount(
      ExternalProtocolHandler::kRememberCheckboxMetric, 0);
  histogram_tester_.ExpectTotalCount(
      ExternalProtocolHandler::kHandleStateMetric, 0);
}
