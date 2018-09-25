// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "third_party/blink/public/common/features.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/select_file_dialog_factory.h"
#include "ui/shell_dialogs/select_file_policy.h"

namespace content {

namespace {

struct SelectFileDialogParams {
  ui::SelectFileDialog::Type type = ui::SelectFileDialog::SELECT_NONE;
};

// A fake ui::SelectFileDialog, which will cancel the file selection instead of
// selecting a file.
class CancellingSelectFileDialog : public ui::SelectFileDialog {
 public:
  CancellingSelectFileDialog(Listener* listener,
                             std::unique_ptr<ui::SelectFilePolicy> policy)
      : ui::SelectFileDialog(listener, std::move(policy)) {}

 protected:
  void SelectFileImpl(Type type,
                      const base::string16& title,
                      const base::FilePath& default_path,
                      const FileTypeInfo* file_types,
                      int file_type_index,
                      const base::FilePath::StringType& default_extension,
                      gfx::NativeWindow owning_window,
                      void* params) override {
    listener_->FileSelectionCanceled(params);
  }

  bool IsRunning(gfx::NativeWindow owning_window) const override {
    return false;
  }
  void ListenerDestroyed() override {}
  bool HasMultipleFileTypeChoicesImpl() override { return false; }

 private:
  ~CancellingSelectFileDialog() override = default;
};

class CancellingSelectFileDialogFactory : public ui::SelectFileDialogFactory {
 public:
  CancellingSelectFileDialogFactory() {}

  ui::SelectFileDialog* Create(
      ui::SelectFileDialog::Listener* listener,
      std::unique_ptr<ui::SelectFilePolicy> policy) override {
    return new CancellingSelectFileDialog(listener, std::move(policy));
  }
};

// A fake ui::SelectFileDialog, which will select one or more pre-determined
// files.
class FakeSelectFileDialog : public ui::SelectFileDialog {
 public:
  FakeSelectFileDialog(std::vector<base::FilePath> result,
                       SelectFileDialogParams* out_params,
                       Listener* listener,
                       std::unique_ptr<ui::SelectFilePolicy> policy)
      : ui::SelectFileDialog(listener, std::move(policy)),
        result_(std::move(result)),
        out_params_(out_params) {}

 protected:
  void SelectFileImpl(Type type,
                      const base::string16& title,
                      const base::FilePath& default_path,
                      const FileTypeInfo* file_types,
                      int file_type_index,
                      const base::FilePath::StringType& default_extension,
                      gfx::NativeWindow owning_window,
                      void* params) override {
    if (out_params_) {
      out_params_->type = type;
    }
    if (result_.size() == 1)
      listener_->FileSelected(result_[0], 0, params);
    else
      listener_->MultiFilesSelected(result_, params);
  }

  bool IsRunning(gfx::NativeWindow owning_window) const override {
    return false;
  }
  void ListenerDestroyed() override {}
  bool HasMultipleFileTypeChoicesImpl() override { return false; }

 private:
  ~FakeSelectFileDialog() override = default;
  std::vector<base::FilePath> result_;
  SelectFileDialogParams* out_params_;
};

class FakeSelectFileDialogFactory : public ui::SelectFileDialogFactory {
 public:
  explicit FakeSelectFileDialogFactory(std::vector<base::FilePath> result,
                                       SelectFileDialogParams* out_params)
      : result_(std::move(result)), out_params_(out_params) {}

  ui::SelectFileDialog* Create(
      ui::SelectFileDialog::Listener* listener,
      std::unique_ptr<ui::SelectFilePolicy> policy) override {
    return new FakeSelectFileDialog(result_, out_params_, listener,
                                    std::move(policy));
  }

 private:
  std::vector<base::FilePath> result_;
  SelectFileDialogParams* out_params_;
};

}  // namespace

// This browser test implements end-to-end tests for the chooseFileSystemEntry
// API.
class FileSystemChooserBrowserTest : public ContentBrowserTest {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    scoped_feature_list_.InitAndEnableFeature(
        blink::features::kWritableFilesAPI);
    ContentBrowserTest::SetUp();
  }

  void TearDown() override {
    ContentBrowserTest::TearDown();
    ASSERT_TRUE(temp_dir_.Delete());
    ui::SelectFileDialog::SetFactory(nullptr);
  }

  base::FilePath CreateTestFile(const std::string& contents) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::FilePath result;
    EXPECT_TRUE(base::CreateTemporaryFileInDir(temp_dir_.GetPath(), &result));
    EXPECT_EQ(int{contents.size()},
              base::WriteFile(result, contents.data(), contents.size()));
    return result;
  }

  base::FilePath CreateTestDir() {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::FilePath result;
    EXPECT_TRUE(base::CreateTemporaryDirInDir(
        temp_dir_.GetPath(), FILE_PATH_LITERAL("test"), &result));
    return result;
  }

 protected:
  const std::string kBlankHtml = "<!DOCTYPE html><html><body>";
  const GURL kTestUrl = GURL("https://foobar.com/");

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::ScopedTempDir temp_dir_;
};

IN_PROC_BROWSER_TEST_F(FileSystemChooserBrowserTest, CancelDialog) {
  ui::SelectFileDialog::SetFactory(new CancellingSelectFileDialogFactory);
  LoadDataWithBaseURL(shell(), kTestUrl, kBlankHtml, kTestUrl);
  auto result = EvalJs(shell(), "self.chooseFileSystemEntries()");
  EXPECT_TRUE(result.error.find("AbortError") != std::string::npos)
      << result.error;
}

IN_PROC_BROWSER_TEST_F(FileSystemChooserBrowserTest, OpenFile) {
  const std::string file_contents = "hello world!";
  const base::FilePath test_file = CreateTestFile(file_contents);
  SelectFileDialogParams dialog_params;
  ui::SelectFileDialog::SetFactory(
      new FakeSelectFileDialogFactory({test_file}, &dialog_params));
  LoadDataWithBaseURL(shell(), kTestUrl, kBlankHtml, kTestUrl);
  EXPECT_EQ(test_file.BaseName().AsUTF8Unsafe(),
            EvalJs(shell(),
                   "(async () => {"
                   "  let e = await self.chooseFileSystemEntries();"
                   "  self.selected_entry = e;"
                   "  return e.name; })()"));
  EXPECT_EQ(ui::SelectFileDialog::SELECT_OPEN_FILE, dialog_params.type);
  EXPECT_EQ(
      file_contents,
      EvalJs(shell(),
             "(async () => { const file = await self.selected_entry.getFile(); "
             "return await new Response(file).text(); })()"));
}

IN_PROC_BROWSER_TEST_F(FileSystemChooserBrowserTest, SaveFile) {
  const std::string file_contents = "file contents to write";
  const base::FilePath test_file = CreateTestFile("");
  {
    // Delete file, since SaveFile should be able to deal with non-existing
    // files.
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(base::DeleteFile(test_file, false));
  }
  SelectFileDialogParams dialog_params;
  ui::SelectFileDialog::SetFactory(
      new FakeSelectFileDialogFactory({test_file}, &dialog_params));
  LoadDataWithBaseURL(shell(), kTestUrl, kBlankHtml, kTestUrl);
  EXPECT_EQ(test_file.BaseName().AsUTF8Unsafe(),
            EvalJs(shell(),
                   "(async () => {"
                   "  let e = await self.chooseFileSystemEntries("
                   "      {type: 'saveFile'});"
                   "  self.entry = e;"
                   "  return e.name; })()"));
  EXPECT_EQ(ui::SelectFileDialog::SELECT_SAVEAS_FILE, dialog_params.type);
  EXPECT_EQ(int{file_contents.size()},
            EvalJs(shell(),
                   JsReplace("(async () => {"
                             "  const w = await self.entry.createWriter();"
                             "  await w.write(0, new Blob([$1]));"
                             "  return (await self.entry.getFile()).size; })()",
                             file_contents)));
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    std::string read_contents;
    EXPECT_TRUE(base::ReadFileToString(test_file, &read_contents));
    EXPECT_EQ(file_contents, read_contents);
  }
}

IN_PROC_BROWSER_TEST_F(FileSystemChooserBrowserTest, OpenMultipleFiles) {
  const base::FilePath test_file1 = CreateTestFile("file1");
  const base::FilePath test_file2 = CreateTestFile("file2");
  SelectFileDialogParams dialog_params;
  ui::SelectFileDialog::SetFactory(new FakeSelectFileDialogFactory(
      {test_file1, test_file2}, &dialog_params));
  LoadDataWithBaseURL(shell(), kTestUrl, kBlankHtml, kTestUrl);
  EXPECT_EQ(ListValueOf(test_file1.BaseName().AsUTF8Unsafe(),
                        test_file2.BaseName().AsUTF8Unsafe()),
            EvalJs(shell(),
                   "(async () => {"
                   "  let e = await self.chooseFileSystemEntries("
                   "      {multiple: true});"
                   "  return e.map(x => x.name); })()"));
  EXPECT_EQ(ui::SelectFileDialog::SELECT_OPEN_MULTI_FILE, dialog_params.type);
}

IN_PROC_BROWSER_TEST_F(FileSystemChooserBrowserTest, OpenDirectory) {
  base::FilePath test_dir = CreateTestDir();
  SelectFileDialogParams dialog_params;
  ui::SelectFileDialog::SetFactory(
      new FakeSelectFileDialogFactory({test_dir}, &dialog_params));
  LoadDataWithBaseURL(shell(), kTestUrl, kBlankHtml, kTestUrl);
  EXPECT_EQ(test_dir.BaseName().AsUTF8Unsafe(),
            EvalJs(shell(),
                   "(async () => {"
                   "  let e = await self.chooseFileSystemEntries("
                   "      {type: 'openDirectory'});"
                   "  self.selected_entry = e;"
                   "  return e.name; })()"));
  EXPECT_EQ(ui::SelectFileDialog::SELECT_FOLDER, dialog_params.type);
}

}  // namespace content