// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/media_router/media_router_file_dialog.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::SaveArg;
using testing::Test;

namespace media_router {

class MockDelegate
    : public MediaRouterFileDialog::MediaRouterFileDialogDelegate {
 public:
  MOCK_METHOD1(FileDialogFileSelected,
               void(const ui::SelectedFileInfo& file_info));
  MOCK_METHOD1(FileDialogSelectionFailed,
               void(const MediaRouterFileDialog::FailureReason reason));
};

class MediaRouterFileDialogTest : public Test {
 public:
  MediaRouterFileDialogTest() {
    scoped_feature_list_.InitFromCommandLine(
        "EnableCastLocalMedia" /* enabled features */,
        std::string() /* disabled features */);
  }

  void SetUp() override {
    dialog_ = base::MakeUnique<MediaRouterFileDialog>(&mock_delegate_);
  }

 protected:
  MockDelegate mock_delegate_;
  std::unique_ptr<MediaRouterFileDialog> dialog_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(MediaRouterFileDialogTest, SelectFileSuccess) {
  // File selection succeeds, success callback called with the right file info.
  // Selected file URL is set properly.
  const std::string path_string = "im/a/path";
  const base::FilePath path = base::FilePath(FILE_PATH_LITERAL("im/a/path"));
  ui::SelectedFileInfo file_info;
  EXPECT_CALL(mock_delegate_, FileDialogFileSelected(_))
      .WillOnce(SaveArg<0>(&file_info));

  dialog_->FileSelected(path, 0, 0);

  ASSERT_EQ(file_info.local_path, path);
  ASSERT_TRUE(dialog_->GetLastSelectedFileUrl().GetContent().find(
                  path_string) != std::string::npos);
}

TEST_F(MediaRouterFileDialogTest, SelectFileCanceled) {
  // File selection gets cancelled, failure callback called
  EXPECT_CALL(mock_delegate_, FileDialogSelectionFailed(dialog_->CANCELED));

  dialog_->FileSelectionCanceled(0);
}

// TODO(offenwanger): Add tests for invalid files when that code is added.

}  // namespace media_router
