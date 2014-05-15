// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/message_loop/message_loop.h"
#include "base/observer_list.h"
#include "base/prefs/pref_service.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/value_conversions.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_extensions.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/download/download_target_determiner.h"
#include "chrome/browser/download/download_target_info.h"
#include "chrome/browser/history/history_service.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_pref_service_syncable.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/download_interrupt_reasons.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/test/mock_download_item.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/common/extension.h"
#include "net/base/mime_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(ENABLE_PLUGINS)
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/plugin_service_filter.h"
#include "content/public/common/webplugininfo.h"
#endif

using ::testing::AnyNumber;
using ::testing::Invoke;
using ::testing::Ref;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::ReturnRefOfCopy;
using ::testing::Truly;
using ::testing::WithArg;
using ::testing::_;
using content::DownloadItem;

namespace {

// No-op delegate.
class NullWebContentsDelegate : public content::WebContentsDelegate {
 public:
  NullWebContentsDelegate() {}
  virtual ~NullWebContentsDelegate() {}
};

// Google Mock action that posts a task to the current message loop that invokes
// the first argument of the mocked method as a callback. Said argument must be
// a base::Callback<void(ParamType)>. |result| must be of |ParamType| and is
// bound as that parameter.
// Example:
//   class FooClass {
//    public:
//     virtual void Foo(base::Callback<void(bool)> callback);
//   };
//   ...
//   EXPECT_CALL(mock_fooclass_instance, Foo(callback))
//     .WillOnce(ScheduleCallback(false));
ACTION_P(ScheduleCallback, result0) {
  base::MessageLoop::current()->PostTask(FROM_HERE, base::Bind(arg0, result0));
}

// Similar to ScheduleCallback, but binds 2 arguments.
ACTION_P2(ScheduleCallback2, result0, result1) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(arg0, result0, result1));
}

// Used with DownloadTestCase. Indicates the type of test case. The expectations
// for the test is set based on the type.
enum TestCaseType {
  SAVE_AS,
  AUTOMATIC,
  FORCED  // Requires that forced_file_path be non-empty.
};

// Used with DownloadTestCase. Type of intermediate filename to expect.
enum TestCaseExpectIntermediate {
  EXPECT_CRDOWNLOAD,   // Expect path/to/target.crdownload.
  EXPECT_UNCONFIRMED,  // Expect path/to/Unconfirmed xxx.crdownload.
  EXPECT_LOCAL_PATH,   // Expect target path.
};

// Typical download test case. Used with
// DownloadTargetDeterminerTest::RunTestCase().
struct DownloadTestCase {
  // Type of test.
  TestCaseType test_type;

  // Expected danger type. Verified at the end of target determination.
  content::DownloadDangerType expected_danger_type;

  // Value of DownloadItem::GetURL()
  const char* url;

  // Value of DownloadItem::GetMimeType()
  const char* mime_type;

  // Should be non-empty if |test_type| == FORCED. Value of GetForcedFilePath().
  const base::FilePath::CharType* forced_file_path;

  // Expected local path. Specified relative to the test download path.
  const base::FilePath::CharType* expected_local_path;

  // Expected target disposition. If this is TARGET_DISPOSITION_PROMPT, then the
  // test run will expect ChromeDownloadManagerDelegate to prompt the user for a
  // download location.
  DownloadItem::TargetDisposition expected_disposition;

  // Type of intermediate path to expect.
  TestCaseExpectIntermediate expected_intermediate;
};

class MockDownloadTargetDeterminerDelegate
    : public DownloadTargetDeterminerDelegate {
 public:
  MOCK_METHOD3(CheckDownloadUrl,
               void(content::DownloadItem*, const base::FilePath&,
                    const CheckDownloadUrlCallback&));
  MOCK_METHOD3(NotifyExtensions,
               void(content::DownloadItem*, const base::FilePath&,
                    const NotifyExtensionsCallback&));
  MOCK_METHOD3(PromptUserForDownloadPath,
               void(content::DownloadItem*, const base::FilePath&,
                    const FileSelectedCallback&));
  MOCK_METHOD3(DetermineLocalPath,
               void(DownloadItem*, const base::FilePath&,
                    const LocalPathCallback&));
  MOCK_METHOD5(ReserveVirtualPath,
               void(DownloadItem*, const base::FilePath&, bool,
                    DownloadPathReservationTracker::FilenameConflictAction,
                    const ReservedPathCallback&));
  MOCK_METHOD2(GetFileMimeType,
               void(const base::FilePath&,
                    const GetFileMimeTypeCallback&));

  void SetupDefaults() {
    ON_CALL(*this, CheckDownloadUrl(_, _, _))
        .WillByDefault(WithArg<2>(
            ScheduleCallback(content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS)));
    ON_CALL(*this, NotifyExtensions(_, _, _))
        .WillByDefault(WithArg<2>(
            ScheduleCallback2(base::FilePath(),
                              DownloadPathReservationTracker::UNIQUIFY)));
    ON_CALL(*this, ReserveVirtualPath(_, _, _, _, _))
        .WillByDefault(Invoke(
            &MockDownloadTargetDeterminerDelegate::NullReserveVirtualPath));
    ON_CALL(*this, PromptUserForDownloadPath(_, _, _))
        .WillByDefault(Invoke(
            &MockDownloadTargetDeterminerDelegate::NullPromptUser));
    ON_CALL(*this, DetermineLocalPath(_, _, _))
        .WillByDefault(Invoke(
            &MockDownloadTargetDeterminerDelegate::NullDetermineLocalPath));
    ON_CALL(*this, GetFileMimeType(_, _))
        .WillByDefault(WithArg<1>(
            ScheduleCallback("")));
  }
 private:
  static void NullReserveVirtualPath(
      DownloadItem* download,
      const base::FilePath& virtual_path,
      bool create_directory,
      DownloadPathReservationTracker::FilenameConflictAction conflict_action,
      const DownloadTargetDeterminerDelegate::ReservedPathCallback& callback);
  static void NullPromptUser(
      DownloadItem* download, const base::FilePath& suggested_path,
      const FileSelectedCallback& callback);
  static void NullDetermineLocalPath(
      DownloadItem* download, const base::FilePath& virtual_path,
      const LocalPathCallback& callback);
};

class DownloadTargetDeterminerTest : public ChromeRenderViewHostTestHarness {
 public:
  // ::testing::Test
  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

  // Creates MockDownloadItem and sets up default expectations.
  content::MockDownloadItem* CreateActiveDownloadItem(
      int32 id,
      const DownloadTestCase& test_case);

  // Sets the AutoOpenBasedOnExtension user preference for |path|.
  void EnableAutoOpenBasedOnExtension(const base::FilePath& path);

  // Set the kDownloadDefaultDirectory managed preference to |path|.
  void SetManagedDownloadPath(const base::FilePath& path);

  // Set the kPromptForDownload user preference to |prompt|.
  void SetPromptForDownload(bool prompt);

  // Given the relative path |path|, returns the full path under the temporary
  // downloads directory.
  base::FilePath GetPathInDownloadDir(const base::FilePath::StringType& path);

  // Run |test_case| using |item|.
  void RunTestCase(const DownloadTestCase& test_case,
                   const base::FilePath& initial_virtual_path,
                   content::MockDownloadItem* item);

  // Runs |test_case| with |item|. When the DownloadTargetDeterminer is done,
  // returns the resulting DownloadTargetInfo.
  scoped_ptr<DownloadTargetInfo> RunDownloadTargetDeterminer(
      const base::FilePath& initial_virtual_path,
      content::MockDownloadItem* item);

  // Run through |test_case_count| tests in |test_cases|. A new MockDownloadItem
  // will be created for each test case and destroyed when the test case is
  // complete.
  void RunTestCasesWithActiveItem(const DownloadTestCase test_cases[],
                                  size_t test_case_count);

  // Verifies that |target_path|, |disposition|, |expected_danger_type| and
  // |intermediate_path| matches the expectations of |test_case|. Posts
  // |closure| to the current message loop when done.
  void VerifyDownloadTarget(const DownloadTestCase& test_case,
                            const DownloadTargetInfo* target_info);

  const base::FilePath& test_download_dir() const {
    return test_download_dir_.path();
  }

  const base::FilePath& test_virtual_dir() const {
    return test_virtual_dir_;
  }

  MockDownloadTargetDeterminerDelegate* delegate() {
    return &delegate_;
  }

  DownloadPrefs* download_prefs() {
    return download_prefs_.get();
  }

 private:
  scoped_ptr<DownloadPrefs> download_prefs_;
  ::testing::NiceMock<MockDownloadTargetDeterminerDelegate> delegate_;
  NullWebContentsDelegate web_contents_delegate_;
  base::ScopedTempDir test_download_dir_;
  base::FilePath test_virtual_dir_;
};

void DownloadTargetDeterminerTest::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();
  CHECK(profile());
  download_prefs_.reset(new DownloadPrefs(profile()));
  web_contents()->SetDelegate(&web_contents_delegate_);
  ASSERT_TRUE(test_download_dir_.CreateUniqueTempDir());
  test_virtual_dir_ = test_download_dir().Append(FILE_PATH_LITERAL("virtual"));
  download_prefs_->SetDownloadPath(test_download_dir());
  delegate_.SetupDefaults();
}

void DownloadTargetDeterminerTest::TearDown() {
  download_prefs_.reset();
  ChromeRenderViewHostTestHarness::TearDown();
}

content::MockDownloadItem*
DownloadTargetDeterminerTest::CreateActiveDownloadItem(
    int32 id,
    const DownloadTestCase& test_case) {
  content::MockDownloadItem* item =
      new ::testing::NiceMock<content::MockDownloadItem>();
  GURL download_url(test_case.url);
  std::vector<GURL> url_chain;
  url_chain.push_back(download_url);
  base::FilePath forced_file_path =
      GetPathInDownloadDir(test_case.forced_file_path);
  DownloadItem::TargetDisposition initial_disposition =
      (test_case.test_type == SAVE_AS) ?
      DownloadItem::TARGET_DISPOSITION_PROMPT :
      DownloadItem::TARGET_DISPOSITION_OVERWRITE;
  EXPECT_EQ(test_case.test_type == FORCED,
            !forced_file_path.empty());

  ON_CALL(*item, GetBrowserContext())
      .WillByDefault(Return(profile()));
  ON_CALL(*item, GetDangerType())
      .WillByDefault(Return(content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS));
  ON_CALL(*item, GetForcedFilePath())
      .WillByDefault(ReturnRefOfCopy(forced_file_path));
  ON_CALL(*item, GetFullPath())
      .WillByDefault(ReturnRefOfCopy(base::FilePath()));
  ON_CALL(*item, GetHash())
      .WillByDefault(ReturnRefOfCopy(std::string()));
  ON_CALL(*item, GetId())
      .WillByDefault(Return(id));
  ON_CALL(*item, GetLastReason())
      .WillByDefault(Return(content::DOWNLOAD_INTERRUPT_REASON_NONE));
  ON_CALL(*item, GetMimeType())
      .WillByDefault(Return(test_case.mime_type));
  ON_CALL(*item, GetReferrerUrl())
      .WillByDefault(ReturnRefOfCopy(download_url));
  ON_CALL(*item, GetState())
      .WillByDefault(Return(DownloadItem::IN_PROGRESS));
  ON_CALL(*item, GetTargetDisposition())
      .WillByDefault(Return(initial_disposition));
  ON_CALL(*item, GetTargetFilePath())
      .WillByDefault(ReturnRefOfCopy(base::FilePath()));
  ON_CALL(*item, GetTransitionType())
      .WillByDefault(Return(content::PAGE_TRANSITION_LINK));
  ON_CALL(*item, GetURL())
      .WillByDefault(ReturnRefOfCopy(download_url));
  ON_CALL(*item, GetUrlChain())
      .WillByDefault(ReturnRefOfCopy(url_chain));
  ON_CALL(*item, GetWebContents())
      .WillByDefault(Return(web_contents()));
  ON_CALL(*item, HasUserGesture())
      .WillByDefault(Return(true));
  ON_CALL(*item, IsDangerous())
      .WillByDefault(Return(false));
  ON_CALL(*item, IsTemporary())
      .WillByDefault(Return(false));
  return item;
}

void DownloadTargetDeterminerTest::EnableAutoOpenBasedOnExtension(
    const base::FilePath& path) {
  EXPECT_TRUE(download_prefs_->EnableAutoOpenBasedOnExtension(path));
}

void DownloadTargetDeterminerTest::SetManagedDownloadPath(
    const base::FilePath& path) {
  profile()->GetTestingPrefService()->
      SetManagedPref(prefs::kDownloadDefaultDirectory,
                     base::CreateFilePathValue(path));
}

void DownloadTargetDeterminerTest::SetPromptForDownload(bool prompt) {
  profile()->GetTestingPrefService()->
      SetBoolean(prefs::kPromptForDownload, prompt);
}

base::FilePath DownloadTargetDeterminerTest::GetPathInDownloadDir(
    const base::FilePath::StringType& relative_path) {
  if (relative_path.empty())
    return base::FilePath();
  base::FilePath full_path(test_download_dir().Append(relative_path));
  return full_path.NormalizePathSeparators();
}

void DownloadTargetDeterminerTest::RunTestCase(
    const DownloadTestCase& test_case,
    const base::FilePath& initial_virtual_path,
    content::MockDownloadItem* item) {
  scoped_ptr<DownloadTargetInfo> target_info =
      RunDownloadTargetDeterminer(initial_virtual_path, item);
  VerifyDownloadTarget(test_case, target_info.get());
}

void CompletionCallbackWrapper(
    const base::Closure& closure,
    scoped_ptr<DownloadTargetInfo>* target_info_receiver,
    scoped_ptr<DownloadTargetInfo> target_info) {
  target_info_receiver->swap(target_info);
  base::MessageLoop::current()->PostTask(FROM_HERE, closure);
}

scoped_ptr<DownloadTargetInfo>
DownloadTargetDeterminerTest::RunDownloadTargetDeterminer(
    const base::FilePath& initial_virtual_path,
    content::MockDownloadItem* item) {
  scoped_ptr<DownloadTargetInfo> target_info;
  base::RunLoop run_loop;
  DownloadTargetDeterminer::Start(
      item, initial_virtual_path, download_prefs_.get(), delegate(),
      base::Bind(&CompletionCallbackWrapper,
                 run_loop.QuitClosure(),
                 &target_info));
  run_loop.Run();
  ::testing::Mock::VerifyAndClearExpectations(delegate());
  return target_info.Pass();
}

void DownloadTargetDeterminerTest::RunTestCasesWithActiveItem(
    const DownloadTestCase test_cases[],
    size_t test_case_count) {
  for (size_t i = 0; i < test_case_count; ++i) {
    scoped_ptr<content::MockDownloadItem> item(
        CreateActiveDownloadItem(i, test_cases[i]));
    SCOPED_TRACE(testing::Message() << "Running test case " << i);
    RunTestCase(test_cases[i], base::FilePath(), item.get());
  }
}

void DownloadTargetDeterminerTest::VerifyDownloadTarget(
    const DownloadTestCase& test_case,
    const DownloadTargetInfo* target_info) {
  base::FilePath expected_local_path(
      GetPathInDownloadDir(test_case.expected_local_path));
  EXPECT_EQ(expected_local_path.value(), target_info->target_path.value());
  EXPECT_EQ(test_case.expected_disposition, target_info->target_disposition);
  EXPECT_EQ(test_case.expected_danger_type, target_info->danger_type);

  switch (test_case.expected_intermediate) {
    case EXPECT_CRDOWNLOAD:
      EXPECT_EQ(DownloadTargetDeterminer::GetCrDownloadPath(
                    target_info->target_path).value(),
                target_info->intermediate_path.value());
      break;

    case EXPECT_UNCONFIRMED:
      // The paths (in English) look like: /path/Unconfirmed xxx.crdownload.
      // Of this, we only check that the path is:
      // 1. Not "/path/target.crdownload",
      // 2. Points to the same directory as the target.
      // 3. Has extension ".crdownload".
      // 4. Basename starts with "Unconfirmed ".
      EXPECT_NE(DownloadTargetDeterminer::GetCrDownloadPath(expected_local_path)
                    .value(),
                target_info->intermediate_path.value());
      EXPECT_EQ(expected_local_path.DirName().value(),
                target_info->intermediate_path.DirName().value());
      EXPECT_TRUE(target_info->intermediate_path.MatchesExtension(
          FILE_PATH_LITERAL(".crdownload")));
      EXPECT_EQ(0u,
                target_info->intermediate_path.BaseName().value().find(
                    FILE_PATH_LITERAL("Unconfirmed ")));
      break;

    case EXPECT_LOCAL_PATH:
      EXPECT_EQ(expected_local_path.value(),
                target_info->intermediate_path.value());
      break;
  }
}

// static
void MockDownloadTargetDeterminerDelegate::NullReserveVirtualPath(
    DownloadItem* download,
    const base::FilePath& virtual_path,
    bool create_directory,
    DownloadPathReservationTracker::FilenameConflictAction conflict_action,
    const DownloadTargetDeterminerDelegate::ReservedPathCallback& callback) {
  callback.Run(virtual_path, true);
}

// static
void MockDownloadTargetDeterminerDelegate::NullPromptUser(
    DownloadItem* download, const base::FilePath& suggested_path,
    const FileSelectedCallback& callback) {
  callback.Run(suggested_path);
}

// static
void MockDownloadTargetDeterminerDelegate::NullDetermineLocalPath(
    DownloadItem* download, const base::FilePath& virtual_path,
    const LocalPathCallback& callback) {
  callback.Run(virtual_path);
}

// NotifyExtensions implementation that overrides the path so that the target
// file is in a subdirectory called 'overridden'. If the extension is '.remove',
// the extension is removed.
void NotifyExtensionsOverridePath(
    content::DownloadItem* download,
    const base::FilePath& path,
    const DownloadTargetDeterminerDelegate::NotifyExtensionsCallback&
        callback) {
  base::FilePath new_path =
      base::FilePath()
      .AppendASCII("overridden")
      .Append(path.BaseName());
  if (new_path.MatchesExtension(FILE_PATH_LITERAL(".remove")))
    new_path = new_path.RemoveExtension();
  callback.Run(new_path, DownloadPathReservationTracker::UNIQUIFY);
}

void CheckDownloadUrlCheckExes(
    content::DownloadItem* download,
    const base::FilePath& path,
    const DownloadTargetDeterminerDelegate::CheckDownloadUrlCallback&
        callback) {
  if (path.MatchesExtension(FILE_PATH_LITERAL(".exe")))
    callback.Run(content::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT);
  else
    callback.Run(content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS);
}

TEST_F(DownloadTargetDeterminerTest, TargetDeterminer_Basic) {
  const DownloadTestCase kBasicTestCases[] = {
    {
      // 0: Automatic Safe
      AUTOMATIC,
      content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
      "http://example.com/foo.txt", "text/plain",
      FILE_PATH_LITERAL(""),

      FILE_PATH_LITERAL("foo.txt"),
      DownloadItem::TARGET_DISPOSITION_OVERWRITE,

      EXPECT_CRDOWNLOAD
    },

    {
      // 1: Save_As Safe
      SAVE_AS,
      content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
      "http://example.com/foo.txt", "text/plain",
      FILE_PATH_LITERAL(""),

      FILE_PATH_LITERAL("foo.txt"),
      DownloadItem::TARGET_DISPOSITION_PROMPT,

      EXPECT_CRDOWNLOAD
    },

    {
      // 2: Automatic Dangerous
      AUTOMATIC,
      content::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE,
      "http://example.com/foo.crx", "",
      FILE_PATH_LITERAL(""),

      FILE_PATH_LITERAL("foo.crx"),
      DownloadItem::TARGET_DISPOSITION_OVERWRITE,

      EXPECT_UNCONFIRMED
    },

    {
      // 3: Forced Safe
      FORCED,
      content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
      "http://example.com/foo.txt", "",
      FILE_PATH_LITERAL("forced-foo.txt"),

      FILE_PATH_LITERAL("forced-foo.txt"),
      DownloadItem::TARGET_DISPOSITION_OVERWRITE,

      EXPECT_LOCAL_PATH
    },
  };

  // The test assumes that .crx files have a danger level of
  // ALLOW_ON_USER_GESTURE.
  ASSERT_EQ(download_util::ALLOW_ON_USER_GESTURE,
            download_util::GetFileDangerLevel(
                base::FilePath(FILE_PATH_LITERAL("foo.crx"))));
  RunTestCasesWithActiveItem(kBasicTestCases, arraysize(kBasicTestCases));
}

TEST_F(DownloadTargetDeterminerTest, TargetDeterminer_CancelSaveAs) {
  const DownloadTestCase kCancelSaveAsTestCases[] = {
    {
      // 0: Save_As Safe, Cancelled.
      SAVE_AS,
      content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
      "http://example.com/foo.txt", "text/plain",
      FILE_PATH_LITERAL(""),

      FILE_PATH_LITERAL(""),
      DownloadItem::TARGET_DISPOSITION_PROMPT,

      EXPECT_LOCAL_PATH
    }
  };
  ON_CALL(*delegate(), PromptUserForDownloadPath(_, _, _))
      .WillByDefault(WithArg<2>(ScheduleCallback(base::FilePath())));
  RunTestCasesWithActiveItem(kCancelSaveAsTestCases,
                             arraysize(kCancelSaveAsTestCases));
}

// The SafeBrowsing check is performed early. Make sure that a download item
// that has been marked as DANGEROUS_URL behaves correctly.
TEST_F(DownloadTargetDeterminerTest, TargetDeterminer_DangerousUrl) {
  const DownloadTestCase kSafeBrowsingTestCases[] = {
    {
      // 0: Automatic Dangerous URL
      AUTOMATIC,
      content::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL,
      "http://phishing.example.com/foo.txt", "",
      FILE_PATH_LITERAL(""),

      FILE_PATH_LITERAL("foo.txt"),
      DownloadItem::TARGET_DISPOSITION_OVERWRITE,

      EXPECT_UNCONFIRMED
    },

    {
      // 1: Save As Dangerous URL
      SAVE_AS,
      content::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL,
      "http://phishing.example.com/foo.txt", "",
      FILE_PATH_LITERAL(""),

      FILE_PATH_LITERAL("foo.txt"),
      DownloadItem::TARGET_DISPOSITION_PROMPT,

      EXPECT_UNCONFIRMED
    },

    {
      // 2: Forced Dangerous URL
      FORCED,
      content::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL,
      "http://phishing.example.com/foo.txt", "",
      FILE_PATH_LITERAL("forced-foo.txt"),

      FILE_PATH_LITERAL("forced-foo.txt"),
      DownloadItem::TARGET_DISPOSITION_OVERWRITE,

      EXPECT_UNCONFIRMED
    },

    {
      // 3: Automatic Dangerous URL + Dangerous file. Dangerous URL takes
      // precedence.
      AUTOMATIC,
      content::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL,
      "http://phishing.example.com/foo.html", "",
      FILE_PATH_LITERAL(""),

      FILE_PATH_LITERAL("foo.html"),
      DownloadItem::TARGET_DISPOSITION_OVERWRITE,

      EXPECT_UNCONFIRMED
    },

    {
      // 4: Save As Dangerous URL + Dangerous file
      SAVE_AS,
      content::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL,
      "http://phishing.example.com/foo.html", "",
      FILE_PATH_LITERAL(""),

      FILE_PATH_LITERAL("foo.html"),
      DownloadItem::TARGET_DISPOSITION_PROMPT,

      EXPECT_UNCONFIRMED
    },

    {
      // 5: Forced Dangerous URL + Dangerous file
      FORCED,
      content::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL,
      "http://phishing.example.com/foo.html", "",
      FILE_PATH_LITERAL("forced-foo.html"),

      FILE_PATH_LITERAL("forced-foo.html"),
      DownloadItem::TARGET_DISPOSITION_OVERWRITE,

      EXPECT_UNCONFIRMED
    },
  };

  ON_CALL(*delegate(), CheckDownloadUrl(_, _, _))
      .WillByDefault(WithArg<2>(ScheduleCallback(
          content::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL)));
  RunTestCasesWithActiveItem(kSafeBrowsingTestCases,
                             arraysize(kSafeBrowsingTestCases));
}

// The SafeBrowsing check is performed early. Make sure that a download item
// that has been marked as MAYBE_DANGEROUS_CONTENT behaves correctly.
TEST_F(DownloadTargetDeterminerTest, TargetDeterminer_MaybeDangerousContent) {
  const DownloadTestCase kSafeBrowsingTestCases[] = {
    {
      // 0: Automatic Maybe dangerous content
      AUTOMATIC,
      content::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT,
      "http://phishing.example.com/foo.exe", "",
      FILE_PATH_LITERAL(""),

      FILE_PATH_LITERAL("foo.exe"),
      DownloadItem::TARGET_DISPOSITION_OVERWRITE,

      EXPECT_UNCONFIRMED
    },

    {
      // 1: Save As Maybe dangerous content
      SAVE_AS,
      content::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT,
      "http://phishing.example.com/foo.exe", "",
      FILE_PATH_LITERAL(""),

      FILE_PATH_LITERAL("foo.exe"),
      DownloadItem::TARGET_DISPOSITION_PROMPT,

      EXPECT_UNCONFIRMED
    },

    {
      // 2: Forced Maybe dangerous content
      FORCED,
      content::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT,
      "http://phishing.example.com/foo.exe", "",
      FILE_PATH_LITERAL("forced-foo.exe"),

      FILE_PATH_LITERAL("forced-foo.exe"),
      DownloadItem::TARGET_DISPOSITION_OVERWRITE,

      EXPECT_UNCONFIRMED
    },
  };

  ON_CALL(*delegate(), CheckDownloadUrl(_, _, _))
      .WillByDefault(WithArg<2>(ScheduleCallback(
          content::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT)));
  RunTestCasesWithActiveItem(kSafeBrowsingTestCases,
                             arraysize(kSafeBrowsingTestCases));
}

// Test whether the last saved directory is used for 'Save As' downloads.
TEST_F(DownloadTargetDeterminerTest, TargetDeterminer_LastSavePath) {
  const DownloadTestCase kLastSavePathTestCasesPre[] = {
    {
      // 0: If the last save path is empty, then the default download directory
      //    should be used.
      SAVE_AS,
      content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
      "http://example.com/foo.txt", "text/plain",
      FILE_PATH_LITERAL(""),

      FILE_PATH_LITERAL("foo.txt"),
      DownloadItem::TARGET_DISPOSITION_PROMPT,

      EXPECT_CRDOWNLOAD
    }
  };

  // These test cases are run with a last save path set to a non-emtpy local
  // download directory.
  const DownloadTestCase kLastSavePathTestCasesPost[] = {
    {
      // 0: This test case is run with the last download directory set to
      //    '<test_download_dir()>/foo'.
      SAVE_AS,
      content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
      "http://example.com/foo.txt", "text/plain",
      FILE_PATH_LITERAL(""),

      FILE_PATH_LITERAL("foo/foo.txt"),
      DownloadItem::TARGET_DISPOSITION_PROMPT,

      EXPECT_CRDOWNLOAD
    },

    {
      // 1: Start an automatic download. This should be saved to the user's
      //    default download directory and not the last used Save As directory.
      AUTOMATIC,
      content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
      "http://example.com/foo.txt", "text/plain",
      FILE_PATH_LITERAL(""),

      FILE_PATH_LITERAL("foo.txt"),
      DownloadItem::TARGET_DISPOSITION_OVERWRITE,

      EXPECT_CRDOWNLOAD
    },
  };

  // This test case is run with the last save path set to a non-empty virtual
  // directory.
  const DownloadTestCase kLastSavePathTestCasesVirtual[] = {
    {
      SAVE_AS,
      content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
      "http://example.com/foo.txt", "text/plain",
      FILE_PATH_LITERAL(""),

      FILE_PATH_LITERAL("bar.txt"),
      DownloadItem::TARGET_DISPOSITION_PROMPT,

      EXPECT_LOCAL_PATH
    },
  };

  {
    SCOPED_TRACE(testing::Message()
                 << "Running with default download path");
    base::FilePath prompt_path =
        GetPathInDownloadDir(FILE_PATH_LITERAL("foo.txt"));
    EXPECT_CALL(*delegate(), PromptUserForDownloadPath(_, prompt_path, _));
    RunTestCasesWithActiveItem(kLastSavePathTestCasesPre,
                               arraysize(kLastSavePathTestCasesPre));
  }

  // Try with a non-empty last save path.
  {
    SCOPED_TRACE(testing::Message()
                 << "Running with local last_selected_directory");
    download_prefs()->SetSaveFilePath(test_download_dir().AppendASCII("foo"));
    base::FilePath prompt_path =
        GetPathInDownloadDir(FILE_PATH_LITERAL("foo/foo.txt"));
    EXPECT_CALL(*delegate(), PromptUserForDownloadPath(_, prompt_path, _));
    RunTestCasesWithActiveItem(kLastSavePathTestCasesPost,
                               arraysize(kLastSavePathTestCasesPost));
  }

  // And again, but this time use a virtual directory.
  {
    SCOPED_TRACE(testing::Message()
                 << "Running with virtual last_selected_directory");
    base::FilePath last_selected_dir = test_virtual_dir().AppendASCII("foo");
    base::FilePath virtual_path = last_selected_dir.AppendASCII("foo.txt");
    download_prefs()->SetSaveFilePath(last_selected_dir);
    EXPECT_CALL(*delegate(), PromptUserForDownloadPath(
        _, last_selected_dir.AppendASCII("foo.txt"), _));
    EXPECT_CALL(*delegate(), DetermineLocalPath(_, virtual_path, _))
        .WillOnce(WithArg<2>(ScheduleCallback(
            GetPathInDownloadDir(FILE_PATH_LITERAL("bar.txt")))));
    RunTestCasesWithActiveItem(kLastSavePathTestCasesVirtual,
                               arraysize(kLastSavePathTestCasesVirtual));
  }
}

// These tests are run with the default downloads folder set to a virtual
// directory.
TEST_F(DownloadTargetDeterminerTest, TargetDeterminer_DefaultVirtual) {
  // The default download directory is the virutal path.
  download_prefs()->SetDownloadPath(test_virtual_dir());

  {
    SCOPED_TRACE(testing::Message() << "Automatic Safe Download");
    const DownloadTestCase kAutomaticDownloadToVirtualDir = {
      AUTOMATIC,
      content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
      "http://example.com/foo.txt", "text/plain",
      FILE_PATH_LITERAL(""),

      FILE_PATH_LITERAL("foo-local.txt"),
      DownloadItem::TARGET_DISPOSITION_OVERWRITE,

      EXPECT_LOCAL_PATH
    };
    EXPECT_CALL(*delegate(), DetermineLocalPath(_, _, _))
        .WillOnce(WithArg<2>(ScheduleCallback(
            GetPathInDownloadDir(FILE_PATH_LITERAL("foo-local.txt")))));
    RunTestCasesWithActiveItem(&kAutomaticDownloadToVirtualDir, 1);
  }

  {
    SCOPED_TRACE(testing::Message() << "Save As to virtual directory");
    const DownloadTestCase kSaveAsToVirtualDir = {
      SAVE_AS,
      content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
      "http://example.com/bar.txt", "text/plain",
      FILE_PATH_LITERAL(""),

      FILE_PATH_LITERAL("foo-local.txt"),
      DownloadItem::TARGET_DISPOSITION_PROMPT,

      EXPECT_LOCAL_PATH
    };
    EXPECT_CALL(*delegate(), DetermineLocalPath(_, _, _))
        .WillOnce(WithArg<2>(ScheduleCallback(
            GetPathInDownloadDir(FILE_PATH_LITERAL("foo-local.txt")))));
    EXPECT_CALL(*delegate(), PromptUserForDownloadPath(
        _, test_virtual_dir().AppendASCII("bar.txt"), _))
        .WillOnce(WithArg<2>(ScheduleCallback(
            test_virtual_dir().AppendASCII("prompted.txt"))));
    RunTestCasesWithActiveItem(&kSaveAsToVirtualDir, 1);
  }

  {
    SCOPED_TRACE(testing::Message() << "Save As to local directory");
    const DownloadTestCase kSaveAsToLocalDir = {
      SAVE_AS,
      content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
      "http://example.com/bar.txt", "text/plain",
      FILE_PATH_LITERAL(""),

      FILE_PATH_LITERAL("foo-x.txt"),
      DownloadItem::TARGET_DISPOSITION_PROMPT,

      EXPECT_CRDOWNLOAD
    };
    EXPECT_CALL(*delegate(), PromptUserForDownloadPath(
        _, test_virtual_dir().AppendASCII("bar.txt"), _))
        .WillOnce(WithArg<2>(ScheduleCallback(
            GetPathInDownloadDir(FILE_PATH_LITERAL("foo-x.txt")))));
    RunTestCasesWithActiveItem(&kSaveAsToLocalDir, 1);
  }

  {
    SCOPED_TRACE(testing::Message() << "Forced safe download");
    const DownloadTestCase kForcedSafe = {
      FORCED,
      content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
      "http://example.com/foo.txt", "",
      FILE_PATH_LITERAL("forced-foo.txt"),

      FILE_PATH_LITERAL("forced-foo.txt"),
      DownloadItem::TARGET_DISPOSITION_OVERWRITE,

      EXPECT_LOCAL_PATH
    };
    RunTestCasesWithActiveItem(&kForcedSafe, 1);
  }
}

// Test that an inactive download will still get a virtual or local download
// path.
TEST_F(DownloadTargetDeterminerTest, TargetDeterminer_InactiveDownload) {
  const DownloadTestCase kInactiveTestCases[] = {
    {
      AUTOMATIC,
      content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
      "http://example.com/foo.txt", "text/plain",
      FILE_PATH_LITERAL(""),

      FILE_PATH_LITERAL(""),
      DownloadItem::TARGET_DISPOSITION_OVERWRITE,

      EXPECT_LOCAL_PATH
    },

    {
      SAVE_AS,
      content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
      "http://example.com/foo.txt", "text/plain",
      FILE_PATH_LITERAL(""),

      FILE_PATH_LITERAL(""),
      DownloadItem::TARGET_DISPOSITION_PROMPT,

      EXPECT_LOCAL_PATH
    }
  };

  for (size_t i = 0; i < arraysize(kInactiveTestCases); ++i) {
    SCOPED_TRACE(testing::Message() << "Running test case " << i);
    const DownloadTestCase& test_case = kInactiveTestCases[i];
    scoped_ptr<content::MockDownloadItem> item(
        CreateActiveDownloadItem(i, test_case));
    EXPECT_CALL(*item.get(), GetState())
        .WillRepeatedly(Return(content::DownloadItem::CANCELLED));
    // Even though one is a SAVE_AS download, no prompt will be displayed to
    // the user because the download is inactive.
    EXPECT_CALL(*delegate(), PromptUserForDownloadPath(_, _, _))
        .Times(0);
    RunTestCase(test_case, base::FilePath(), item.get());
  }
}

// If the reserved path could not be verified, then the user should see a
// prompt.
TEST_F(DownloadTargetDeterminerTest, TargetDeterminer_ReservationFailed) {
  const DownloadTestCase kReservationFailedCases[] = {
    {
      // 0: Automatic download. Since the reservation fails, the disposition of
      // the target is to prompt, but the returned path is used.
      AUTOMATIC,
      content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
      "http://example.com/foo.txt", "text/plain",
      FILE_PATH_LITERAL(""),

      FILE_PATH_LITERAL("bar.txt"),
      DownloadItem::TARGET_DISPOSITION_PROMPT,

      EXPECT_CRDOWNLOAD
    },
  };

  // Setup ReserveVirtualPath() to fail.
  ON_CALL(*delegate(), ReserveVirtualPath(_, _, _, _, _))
      .WillByDefault(WithArg<4>(ScheduleCallback2(
          GetPathInDownloadDir(FILE_PATH_LITERAL("bar.txt")), false)));
  RunTestCasesWithActiveItem(kReservationFailedCases,
                             arraysize(kReservationFailedCases));
}

// If the local path could not be determined, the download should be cancelled.
TEST_F(DownloadTargetDeterminerTest, TargetDeterminer_LocalPathFailed) {
  const DownloadTestCase kLocalPathFailedCases[] = {
    {
      // 0: Automatic download.
      AUTOMATIC,
      content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
      "http://example.com/foo.txt", "text/plain",
      FILE_PATH_LITERAL(""),

      FILE_PATH_LITERAL(""),
      DownloadItem::TARGET_DISPOSITION_OVERWRITE,

      EXPECT_LOCAL_PATH
    },
  };

  // The default download directory is the virtual path.
  download_prefs()->SetDownloadPath(test_virtual_dir());
  // Simulate failed call to DetermineLocalPath.
  EXPECT_CALL(*delegate(), DetermineLocalPath(
      _, GetPathInDownloadDir(FILE_PATH_LITERAL("virtual/foo.txt")), _))
      .WillOnce(WithArg<2>(ScheduleCallback(base::FilePath())));
  RunTestCasesWithActiveItem(kLocalPathFailedCases,
                             arraysize(kLocalPathFailedCases));
}

// Downloads that have a danger level of ALLOW_ON_USER_GESTURE should be marked
// as safe depending on whether there was a user gesture associated with the
// download and whether the referrer was visited prior to today.
TEST_F(DownloadTargetDeterminerTest, TargetDeterminer_VisitedReferrer) {
  const DownloadTestCase kVisitedReferrerCases[] = {
    // http://visited.example.com/ is added to the history as a visit that
    // happened prior to today.
    {
      // 0: Safe download due to visiting referrer before.
      AUTOMATIC,
      content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
      "http://visited.example.com/foo.crx", "application/xml",
      FILE_PATH_LITERAL(""),

      FILE_PATH_LITERAL("foo.crx"),
      DownloadItem::TARGET_DISPOSITION_OVERWRITE,

      EXPECT_CRDOWNLOAD
    },

    {
      // 1: Dangerous due to not having visited referrer before.
      AUTOMATIC,
      content::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE,
      "http://not-visited.example.com/foo.crx", "application/xml",
      FILE_PATH_LITERAL(""),

      FILE_PATH_LITERAL("foo.crx"),
      DownloadItem::TARGET_DISPOSITION_OVERWRITE,

      EXPECT_UNCONFIRMED
    },

    {
      // 2: Safe because the user is being prompted.
      SAVE_AS,
      content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
      "http://not-visited.example.com/foo.crx", "application/xml",
      FILE_PATH_LITERAL(""),

      FILE_PATH_LITERAL("foo.crx"),
      DownloadItem::TARGET_DISPOSITION_PROMPT,

      EXPECT_CRDOWNLOAD
    },

    {
      // 3: Safe because of forced path.
      FORCED,
      content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
      "http://not-visited.example.com/foo.crx", "application/xml",
      FILE_PATH_LITERAL("foo.crx"),

      FILE_PATH_LITERAL("foo.crx"),
      DownloadItem::TARGET_DISPOSITION_OVERWRITE,

      EXPECT_LOCAL_PATH
    },
  };

  // This test assumes that the danger level of .crx files is
  // ALLOW_ON_USER_GESTURE.
  ASSERT_EQ(download_util::ALLOW_ON_USER_GESTURE,
            download_util::GetFileDangerLevel(
                base::FilePath(FILE_PATH_LITERAL("foo.crx"))));

  // First the history service must exist.
  ASSERT_TRUE(profile()->CreateHistoryService(false, false));

  GURL url("http://visited.example.com/visited-link.html");
  // The time of visit is picked to be several seconds prior to the most recent
  // midnight.
  base::Time time_of_visit(
      base::Time::Now().LocalMidnight() - base::TimeDelta::FromSeconds(10));
  HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(profile(), Profile::EXPLICIT_ACCESS);
  ASSERT_TRUE(history_service);
  history_service->AddPage(url, time_of_visit, history::SOURCE_BROWSED);

  RunTestCasesWithActiveItem(kVisitedReferrerCases,
                             arraysize(kVisitedReferrerCases));
}

// These test cases are run with "Prompt for download" user preference set to
// true.
TEST_F(DownloadTargetDeterminerTest, TargetDeterminer_PromptAlways) {
  const DownloadTestCase kPromptingTestCases[] = {
    {
      // 0: Safe Automatic - Should prompt because of "Prompt for download"
      //    preference setting.
      AUTOMATIC,
      content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
      "http://example.com/foo.txt", "text/plain",
      FILE_PATH_LITERAL(""),

      FILE_PATH_LITERAL("foo.txt"),
      DownloadItem::TARGET_DISPOSITION_PROMPT,

      EXPECT_CRDOWNLOAD
    },

    {
      // 1: Safe Forced - Shouldn't prompt.
      FORCED,
      content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
      "http://example.com/foo.txt", "text/plain",
      FILE_PATH_LITERAL("foo.txt"),

      FILE_PATH_LITERAL("foo.txt"),
      DownloadItem::TARGET_DISPOSITION_OVERWRITE,

      EXPECT_LOCAL_PATH
    },

    {
      // 2: Automatic - The filename extension is marked as one that we will
      //    open automatically. Shouldn't prompt.
      AUTOMATIC,
      content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
      "http://example.com/foo.dummy", "",
      FILE_PATH_LITERAL(""),

      FILE_PATH_LITERAL("foo.dummy"),
      DownloadItem::TARGET_DISPOSITION_OVERWRITE,

      EXPECT_CRDOWNLOAD
    },
  };

  SetPromptForDownload(true);
  EnableAutoOpenBasedOnExtension(
      base::FilePath(FILE_PATH_LITERAL("dummy.dummy")));
  RunTestCasesWithActiveItem(kPromptingTestCases,
                             arraysize(kPromptingTestCases));
}

#if !defined(OS_ANDROID)
// These test cases are run with "Prompt for download" user preference set to
// true. Automatic extension downloads shouldn't cause prompting.
// Android doesn't support extensions.
TEST_F(DownloadTargetDeterminerTest, TargetDeterminer_PromptAlways_Extension) {
  const DownloadTestCase kPromptingTestCases[] = {
    {
      // 0: Automatic Browser Extension download. - Shouldn't prompt for browser
      //    extension downloads even if "Prompt for download" preference is set.
      AUTOMATIC,
      content::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE,
      "http://example.com/foo.crx",
      extensions::Extension::kMimeType,
      FILE_PATH_LITERAL(""),

      FILE_PATH_LITERAL("foo.crx"),
      DownloadItem::TARGET_DISPOSITION_OVERWRITE,

      EXPECT_UNCONFIRMED
    },

#if defined(OS_WIN)
    {
      // 1: Automatic User Script - Shouldn't prompt for user script downloads
      //    even if "Prompt for download" preference is set. ".js" files are
      //    considered dangerous on Windows.
      AUTOMATIC,
      content::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE,
      "http://example.com/foo.user.js", "",
      FILE_PATH_LITERAL(""),

      FILE_PATH_LITERAL("foo.user.js"),
      DownloadItem::TARGET_DISPOSITION_OVERWRITE,

      EXPECT_UNCONFIRMED
    },
#else
    {
      // 1: Automatic User Script - Shouldn't prompt for user script downloads
      //    even if "Prompt for download" preference is set.
      AUTOMATIC,
      content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
      "http://example.com/foo.user.js", "",
      FILE_PATH_LITERAL(""),

      FILE_PATH_LITERAL("foo.user.js"),
      DownloadItem::TARGET_DISPOSITION_OVERWRITE,

      EXPECT_CRDOWNLOAD
    },
#endif
  };

  SetPromptForDownload(true);
  RunTestCasesWithActiveItem(kPromptingTestCases,
                             arraysize(kPromptingTestCases));
}
#endif

// If the download path is managed, then we don't show any prompts.
// Note that if the download path is managed, then PromptForDownload() is false.
TEST_F(DownloadTargetDeterminerTest, TargetDeterminer_ManagedPath) {
  const DownloadTestCase kManagedPathTestCases[] = {
    {
      // 0: Automatic Safe
      AUTOMATIC,
      content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
      "http://example.com/foo.txt", "text/plain",
      FILE_PATH_LITERAL(""),

      FILE_PATH_LITERAL("foo.txt"),
      DownloadItem::TARGET_DISPOSITION_OVERWRITE,

      EXPECT_CRDOWNLOAD
    },

    {
      // 1: Save_As Safe
      SAVE_AS,
      content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
      "http://example.com/foo.txt", "text/plain",
      FILE_PATH_LITERAL(""),

      FILE_PATH_LITERAL("foo.txt"),
      DownloadItem::TARGET_DISPOSITION_OVERWRITE,

      EXPECT_CRDOWNLOAD
    },
  };

  SetManagedDownloadPath(test_download_dir());
  ASSERT_TRUE(download_prefs()->IsDownloadPathManaged());
  RunTestCasesWithActiveItem(kManagedPathTestCases,
                             arraysize(kManagedPathTestCases));
}

// Test basic functionality supporting extensions that want to override download
// filenames.
TEST_F(DownloadTargetDeterminerTest, TargetDeterminer_NotifyExtensionsSafe) {
  const DownloadTestCase kNotifyExtensionsTestCases[] = {
    {
      // 0: Automatic Safe
      AUTOMATIC,
      content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
      "http://example.com/foo.txt", "text/plain",
      FILE_PATH_LITERAL(""),

      FILE_PATH_LITERAL("overridden/foo.txt"),
      DownloadItem::TARGET_DISPOSITION_OVERWRITE,

      EXPECT_CRDOWNLOAD
    },

    {
      // 1: Save_As Safe
      SAVE_AS,
      content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
      "http://example.com/foo.txt", "text/plain",
      FILE_PATH_LITERAL(""),

      FILE_PATH_LITERAL("overridden/foo.txt"),
      DownloadItem::TARGET_DISPOSITION_PROMPT,

      EXPECT_CRDOWNLOAD
    },

    {
      // 2: Automatic Dangerous
      AUTOMATIC,
      content::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE,
      "http://example.com/foo.crx", "",
      FILE_PATH_LITERAL(""),

      FILE_PATH_LITERAL("overridden/foo.crx"),
      DownloadItem::TARGET_DISPOSITION_OVERWRITE,

      EXPECT_UNCONFIRMED
    },

    {
      // 3: Forced Safe
      FORCED,
      content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
      "http://example.com/foo.txt", "",
      FILE_PATH_LITERAL("forced-foo.txt"),

      FILE_PATH_LITERAL("forced-foo.txt"),
      DownloadItem::TARGET_DISPOSITION_OVERWRITE,

      EXPECT_LOCAL_PATH
    },
  };

  ON_CALL(*delegate(), NotifyExtensions(_, _, _))
      .WillByDefault(Invoke(&NotifyExtensionsOverridePath));
  RunTestCasesWithActiveItem(kNotifyExtensionsTestCases,
                             arraysize(kNotifyExtensionsTestCases));
}

// Test that filenames provided by extensions are passed into SafeBrowsing
// checks and dangerous download checks.
TEST_F(DownloadTargetDeterminerTest, TargetDeterminer_NotifyExtensionsUnsafe) {
  const DownloadTestCase kNotifyExtensionsTestCases[] = {
    {
      // 0: Automatic Safe : Later overridden by a dangerous filetype.
      AUTOMATIC,
      content::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE,
      "http://example.com/foo.crx.remove", "text/plain",
      FILE_PATH_LITERAL(""),

      FILE_PATH_LITERAL("overridden/foo.crx"),
      DownloadItem::TARGET_DISPOSITION_OVERWRITE,

      EXPECT_UNCONFIRMED
    },

    {
      // 1: Automatic Safe : Later overridden by a potentially dangerous
      // filetype.
      AUTOMATIC,
      content::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT,
      "http://example.com/foo.exe.remove", "text/plain",
      FILE_PATH_LITERAL(""),

      FILE_PATH_LITERAL("overridden/foo.exe"),
      DownloadItem::TARGET_DISPOSITION_OVERWRITE,

      EXPECT_UNCONFIRMED
    },
  };

  ON_CALL(*delegate(), NotifyExtensions(_, _, _))
      .WillByDefault(Invoke(&NotifyExtensionsOverridePath));
  ON_CALL(*delegate(), CheckDownloadUrl(_, _, _))
      .WillByDefault(Invoke(&CheckDownloadUrlCheckExes));
  RunTestCasesWithActiveItem(kNotifyExtensionsTestCases,
                             arraysize(kNotifyExtensionsTestCases));
}

// Test that conflict actions set by extensions are passed correctly into
// ReserveVirtualPath.
TEST_F(DownloadTargetDeterminerTest,
       TargetDeterminer_NotifyExtensionsConflict) {
  const DownloadTestCase kNotifyExtensionsTestCase = {
    AUTOMATIC,
    content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
    "http://example.com/foo.txt", "text/plain",
    FILE_PATH_LITERAL(""),

    FILE_PATH_LITERAL("overridden/foo.txt"),
    DownloadItem::TARGET_DISPOSITION_OVERWRITE,

    EXPECT_CRDOWNLOAD
  };

  const DownloadTestCase& test_case = kNotifyExtensionsTestCase;
  scoped_ptr<content::MockDownloadItem> item(
      CreateActiveDownloadItem(0, test_case));
  base::FilePath overridden_path(FILE_PATH_LITERAL("overridden/foo.txt"));
  base::FilePath full_overridden_path =
      GetPathInDownloadDir(overridden_path.value());

  // First case: An extension sets the conflict_action to OVERWRITE.
  EXPECT_CALL(*delegate(), NotifyExtensions(_, _, _))
      .WillOnce(WithArg<2>(
          ScheduleCallback2(overridden_path,
                            DownloadPathReservationTracker::OVERWRITE)));
  EXPECT_CALL(*delegate(), ReserveVirtualPath(
      _, full_overridden_path, true, DownloadPathReservationTracker::OVERWRITE,
      _)).WillOnce(WithArg<4>(
          ScheduleCallback2(full_overridden_path, true)));

  RunTestCase(test_case, base::FilePath(), item.get());

  // Second case: An extension sets the conflict_action to PROMPT.
  EXPECT_CALL(*delegate(), NotifyExtensions(_, _, _))
      .WillOnce(WithArg<2>(
          ScheduleCallback2(overridden_path,
                            DownloadPathReservationTracker::PROMPT)));
  EXPECT_CALL(*delegate(), ReserveVirtualPath(
      _, full_overridden_path, true, DownloadPathReservationTracker::PROMPT, _))
      .WillOnce(WithArg<4>(
          ScheduleCallback2(full_overridden_path, true)));
  RunTestCase(test_case, base::FilePath(), item.get());
}

// Test that relative paths returned by extensions are always relative to the
// default downloads path.
TEST_F(DownloadTargetDeterminerTest,
       TargetDeterminer_NotifyExtensionsDefaultPath) {
  const DownloadTestCase kNotifyExtensionsTestCase = {
    SAVE_AS,
    content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
    "http://example.com/foo.txt", "text/plain",
    FILE_PATH_LITERAL(""),

    FILE_PATH_LITERAL("overridden/foo.txt"),
    DownloadItem::TARGET_DISPOSITION_PROMPT,

    EXPECT_CRDOWNLOAD
  };

  const DownloadTestCase& test_case = kNotifyExtensionsTestCase;
  scoped_ptr<content::MockDownloadItem> item(
      CreateActiveDownloadItem(0, test_case));
  base::FilePath overridden_path(FILE_PATH_LITERAL("overridden/foo.txt"));
  base::FilePath full_overridden_path =
      GetPathInDownloadDir(overridden_path.value());

  download_prefs()->SetSaveFilePath(GetPathInDownloadDir(
      FILE_PATH_LITERAL("last_selected")));

  EXPECT_CALL(*delegate(), NotifyExtensions(_, _, _))
      .WillOnce(WithArg<2>(
          ScheduleCallback2(overridden_path,
                            DownloadPathReservationTracker::UNIQUIFY)));
  EXPECT_CALL(*delegate(),
              PromptUserForDownloadPath(_, full_overridden_path, _))
      .WillOnce(WithArg<2>(
          ScheduleCallback(full_overridden_path)));
  RunTestCase(test_case, base::FilePath(), item.get());
}

TEST_F(DownloadTargetDeterminerTest,
       TargetDeterminer_InitialVirtualPathUnsafe) {
  const base::FilePath::CharType* kInitialPath =
      FILE_PATH_LITERAL("some_path/bar.html");

  const DownloadTestCase kInitialPathTestCase = {
    // 0: Save As Save. The path generated based on the DownloadItem is safe,
    // but the initial path is unsafe. However, the download is not considered
    // dangerous since the user has been prompted.
    SAVE_AS,
    content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
    "http://example.com/foo.txt", "text/plain",
    FILE_PATH_LITERAL(""),

    kInitialPath,
    DownloadItem::TARGET_DISPOSITION_PROMPT,

    EXPECT_CRDOWNLOAD
  };

  const DownloadTestCase& test_case = kInitialPathTestCase;
  scoped_ptr<content::MockDownloadItem> item(
      CreateActiveDownloadItem(1, test_case));
  EXPECT_CALL(*item, GetLastReason())
      .WillRepeatedly(Return(
          content::DOWNLOAD_INTERRUPT_REASON_NETWORK_FAILED));
  EXPECT_CALL(*item, GetTargetDisposition())
      .WillRepeatedly(Return(DownloadItem::TARGET_DISPOSITION_PROMPT));
  RunTestCase(test_case, GetPathInDownloadDir(kInitialPath), item.get());
}

// Prompting behavior for resumed downloads is based on the last interrupt
// reason. If the reason indicates that the target path may not be suitable for
// the download (ACCESS_DENIED, NO_SPACE, etc..), then the user should be
// prompted, and not otherwise. These test cases shouldn't result in prompting
// since the error is set to NETWORK_FAILED.
TEST_F(DownloadTargetDeterminerTest, TargetDeterminer_ResumedNoPrompt) {
  // All test cases run with GetPathInDownloadDir(kInitialPath) as the inital
  // path.
  const base::FilePath::CharType* kInitialPath =
      FILE_PATH_LITERAL("some_path/bar.txt");

  const DownloadTestCase kResumedTestCases[] = {
    {
      // 0: Automatic Safe: Initial path is ignored since the user has not been
      // prompted before.
      AUTOMATIC,
      content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
      "http://example.com/foo.txt", "text/plain",
      FILE_PATH_LITERAL(""),

      FILE_PATH_LITERAL("foo.txt"),
      DownloadItem::TARGET_DISPOSITION_OVERWRITE,

      EXPECT_CRDOWNLOAD
    },

    {
      // 1: Save_As Safe: Initial path used.
      SAVE_AS,
      content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
      "http://example.com/foo.txt", "text/plain",
      FILE_PATH_LITERAL(""),

      kInitialPath,
      DownloadItem::TARGET_DISPOSITION_PROMPT,

      EXPECT_CRDOWNLOAD
    },

    {
      // 2: Automatic Dangerous: Initial path is ignored since the user hasn't
      // been prompted before.
      AUTOMATIC,
      content::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE,
      "http://example.com/foo.crx", "",
      FILE_PATH_LITERAL(""),

      FILE_PATH_LITERAL("foo.crx"),
      DownloadItem::TARGET_DISPOSITION_OVERWRITE,

      EXPECT_UNCONFIRMED
    },

    {
      // 3: Forced Safe: Initial path is ignored due to the forced path.
      FORCED,
      content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
      "http://example.com/foo.txt", "",
      FILE_PATH_LITERAL("forced-foo.txt"),

      FILE_PATH_LITERAL("forced-foo.txt"),
      DownloadItem::TARGET_DISPOSITION_OVERWRITE,

      EXPECT_LOCAL_PATH
    },
  };

  // The test assumes that .crx files have a danger level of
  // ALLOW_ON_USER_GESTURE.
  ASSERT_EQ(download_util::ALLOW_ON_USER_GESTURE,
            download_util::GetFileDangerLevel(
                base::FilePath(FILE_PATH_LITERAL("foo.crx"))));
  for (size_t i = 0; i < arraysize(kResumedTestCases); ++i) {
    SCOPED_TRACE(testing::Message() << "Running test case " << i);
    const DownloadTestCase& test_case = kResumedTestCases[i];
    scoped_ptr<content::MockDownloadItem> item(
        CreateActiveDownloadItem(i, test_case));
    base::FilePath expected_path =
        GetPathInDownloadDir(test_case.expected_local_path);
    ON_CALL(*item.get(), GetLastReason())
        .WillByDefault(Return(
            content::DOWNLOAD_INTERRUPT_REASON_NETWORK_FAILED));
    // Extensions should be notified if a new path is being generated and there
    // is no forced path. In the test cases above, this is true for tests with
    // type == AUTOMATIC.
    EXPECT_CALL(*delegate(), NotifyExtensions(_, _, _))
        .Times(test_case.test_type == AUTOMATIC ? 1 : 0);
    EXPECT_CALL(*delegate(), ReserveVirtualPath(_, expected_path, false, _, _));
    EXPECT_CALL(*delegate(), PromptUserForDownloadPath(_, expected_path, _))
        .Times(0);
    EXPECT_CALL(*delegate(), DetermineLocalPath(_, expected_path, _));
    EXPECT_CALL(*delegate(), CheckDownloadUrl(_, expected_path, _));
    RunTestCase(test_case, GetPathInDownloadDir(kInitialPath), item.get());
  }
}

// Test that a forced download doesn't prompt, even if the interrupt reason
// suggests that the target path may not be suitable for downloads.
TEST_F(DownloadTargetDeterminerTest, TargetDeterminer_ResumedForcedDownload) {
  const base::FilePath::CharType* kInitialPath =
      FILE_PATH_LITERAL("some_path/bar.txt");
  const DownloadTestCase kResumedForcedDownload = {
    // 3: Forced Safe
    FORCED,
    content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
    "http://example.com/foo.txt", "",
    FILE_PATH_LITERAL("forced-foo.txt"),

    FILE_PATH_LITERAL("forced-foo.txt"),
    DownloadItem::TARGET_DISPOSITION_OVERWRITE,

    EXPECT_LOCAL_PATH
  };

  const DownloadTestCase& test_case = kResumedForcedDownload;
  base::FilePath expected_path =
      GetPathInDownloadDir(test_case.expected_local_path);
  scoped_ptr<content::MockDownloadItem> item(
      CreateActiveDownloadItem(0, test_case));
  ON_CALL(*item.get(), GetLastReason())
      .WillByDefault(Return(content::DOWNLOAD_INTERRUPT_REASON_FILE_NO_SPACE));
  EXPECT_CALL(*delegate(), NotifyExtensions(_, _, _))
      .Times(test_case.test_type == AUTOMATIC ? 1 : 0);
  EXPECT_CALL(*delegate(), ReserveVirtualPath(_, expected_path, false, _, _));
  EXPECT_CALL(*delegate(), PromptUserForDownloadPath(_, _, _))
      .Times(0);
  EXPECT_CALL(*delegate(), DetermineLocalPath(_, expected_path, _));
  EXPECT_CALL(*delegate(), CheckDownloadUrl(_, expected_path, _));
  RunTestCase(test_case, GetPathInDownloadDir(kInitialPath), item.get());
}

// Prompting behavior for resumed downloads is based on the last interrupt
// reason. If the reason indicates that the target path may not be suitable for
// the download (ACCESS_DENIED, NO_SPACE, etc..), then the user should be
// prompted, and not otherwise. These test cases result in prompting since the
// error is set to NO_SPACE.
TEST_F(DownloadTargetDeterminerTest, TargetDeterminer_ResumedWithPrompt) {
  // All test cases run with GetPathInDownloadDir(kInitialPath) as the inital
  // path.
  const base::FilePath::CharType* kInitialPath =
      FILE_PATH_LITERAL("some_path/bar.txt");

  const DownloadTestCase kResumedTestCases[] = {
    {
      // 0: Automatic Safe
      AUTOMATIC,
      content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
      "http://example.com/foo.txt", "text/plain",
      FILE_PATH_LITERAL(""),

      FILE_PATH_LITERAL("foo.txt"),
      DownloadItem::TARGET_DISPOSITION_PROMPT,

      EXPECT_CRDOWNLOAD
    },

    {
      // 1: Save_As Safe
      SAVE_AS,
      content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
      "http://example.com/foo.txt", "text/plain",
      FILE_PATH_LITERAL(""),

      kInitialPath,
      DownloadItem::TARGET_DISPOSITION_PROMPT,

      EXPECT_CRDOWNLOAD
    },

    {
      // 2: Automatic Dangerous
      AUTOMATIC,
      content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
      "http://example.com/foo.crx", "",
      FILE_PATH_LITERAL(""),

      FILE_PATH_LITERAL("foo.crx"),
      DownloadItem::TARGET_DISPOSITION_PROMPT,

      EXPECT_CRDOWNLOAD
    },
  };

  // The test assumes that .xml files have a danger level of
  // ALLOW_ON_USER_GESTURE.
  ASSERT_EQ(download_util::ALLOW_ON_USER_GESTURE,
            download_util::GetFileDangerLevel(
                base::FilePath(FILE_PATH_LITERAL("foo.crx"))));
  for (size_t i = 0; i < arraysize(kResumedTestCases); ++i) {
    SCOPED_TRACE(testing::Message() << "Running test case " << i);
    download_prefs()->SetSaveFilePath(test_download_dir());
    const DownloadTestCase& test_case = kResumedTestCases[i];
    base::FilePath expected_path =
        GetPathInDownloadDir(test_case.expected_local_path);
    scoped_ptr<content::MockDownloadItem> item(
        CreateActiveDownloadItem(i, test_case));
    ON_CALL(*item.get(), GetLastReason())
        .WillByDefault(Return(
            content::DOWNLOAD_INTERRUPT_REASON_FILE_NO_SPACE));
    EXPECT_CALL(*delegate(), NotifyExtensions(_, _, _))
        .Times(test_case.test_type == AUTOMATIC ? 1 : 0);
    EXPECT_CALL(*delegate(), ReserveVirtualPath(_, expected_path, false, _, _));
    EXPECT_CALL(*delegate(), PromptUserForDownloadPath(_, expected_path, _));
    EXPECT_CALL(*delegate(), DetermineLocalPath(_, expected_path, _));
    EXPECT_CALL(*delegate(), CheckDownloadUrl(_, expected_path, _));
    RunTestCase(test_case, GetPathInDownloadDir(kInitialPath), item.get());
  }
}

// Test intermediate filename generation for resumed downloads.
TEST_F(DownloadTargetDeterminerTest,
       TargetDeterminer_IntermediateNameForResumed) {
  // All test cases run with GetPathInDownloadDir(kInitialPath) as the inital
  // path.
  const base::FilePath::CharType kInitialPath[] =
      FILE_PATH_LITERAL("some_path/bar.txt");

  struct IntermediateNameTestCase {
    // General test case settings.
    DownloadTestCase general;

    // Value of DownloadItem::GetFullPath() during test run, relative
    // to test download path.
    const base::FilePath::CharType* initial_intermediate_path;

    // Expected intermediate path relatvie to the test download path. An exact
    // match is performed if this string is non-empty. Ignored otherwise.
    const base::FilePath::CharType* expected_intermediate_path;
  } kIntermediateNameTestCases[] = {
    {
      {
        // 0: Automatic Safe
        AUTOMATIC,
        content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
        "http://example.com/foo.txt", "text/plain",
        FILE_PATH_LITERAL(""),

        FILE_PATH_LITERAL("foo.txt"),
        DownloadItem::TARGET_DISPOSITION_OVERWRITE,

        EXPECT_CRDOWNLOAD
      },
      FILE_PATH_LITERAL("bar.txt.crdownload"),
      FILE_PATH_LITERAL("foo.txt.crdownload")
    },

    {
      {
        // 1: Save_As Safe
        SAVE_AS,
        content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
        "http://example.com/foo.txt", "text/plain",
        FILE_PATH_LITERAL(""),

        kInitialPath,
        DownloadItem::TARGET_DISPOSITION_PROMPT,

        EXPECT_CRDOWNLOAD
      },
      FILE_PATH_LITERAL("foo.txt.crdownload"),
      FILE_PATH_LITERAL("some_path/bar.txt.crdownload")
    },

    {
      {
        // 2: Automatic Dangerous
        AUTOMATIC,
        content::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE,
        "http://example.com/foo.crx", "",
        FILE_PATH_LITERAL(""),

        FILE_PATH_LITERAL("foo.crx"),
        DownloadItem::TARGET_DISPOSITION_OVERWRITE,

        EXPECT_UNCONFIRMED
      },
      FILE_PATH_LITERAL("Unconfirmed abcd.crdownload"),
      FILE_PATH_LITERAL("Unconfirmed abcd.crdownload")
    },

    {
      {
        // 3: Automatic Dangerous
        AUTOMATIC,
        content::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE,
        "http://example.com/foo.crx", "",
        FILE_PATH_LITERAL(""),

        FILE_PATH_LITERAL("foo.crx"),
        DownloadItem::TARGET_DISPOSITION_OVERWRITE,

        EXPECT_UNCONFIRMED
      },
      FILE_PATH_LITERAL("other_path/Unconfirmed abcd.crdownload"),
      // Rely on the EXPECT_UNCONFIRMED check in the general test settings. A
      // new intermediate path of the form "Unconfirmed <number>.crdownload"
      // should be generated for this case since the initial intermediate path
      // is in the wrong directory.
      FILE_PATH_LITERAL("")
    },

    {
      {
        // 3: Forced Safe: Initial path is ignored due to the forced path.
        FORCED,
        content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
        "http://example.com/foo.txt", "",
        FILE_PATH_LITERAL("forced-foo.txt"),

        FILE_PATH_LITERAL("forced-foo.txt"),
        DownloadItem::TARGET_DISPOSITION_OVERWRITE,

        EXPECT_LOCAL_PATH
      },
      FILE_PATH_LITERAL("forced-foo.txt"),
      FILE_PATH_LITERAL("forced-foo.txt")
    },
  };

  // The test assumes that .crx files have a danger level of
  // ALLOW_ON_USER_GESTURE.
  ASSERT_EQ(download_util::ALLOW_ON_USER_GESTURE,
            download_util::GetFileDangerLevel(
                base::FilePath(FILE_PATH_LITERAL("foo.crx"))));

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kIntermediateNameTestCases); ++i) {
    SCOPED_TRACE(testing::Message() << "Running test case " << i);
    const IntermediateNameTestCase& test_case = kIntermediateNameTestCases[i];
    scoped_ptr<content::MockDownloadItem> item(
        CreateActiveDownloadItem(i, test_case.general));

    ON_CALL(*item.get(), GetLastReason())
        .WillByDefault(Return(
            content::DOWNLOAD_INTERRUPT_REASON_NETWORK_FAILED));
    ON_CALL(*item.get(), GetFullPath())
        .WillByDefault(ReturnRefOfCopy(
            GetPathInDownloadDir(test_case.initial_intermediate_path)));
    ON_CALL(*item.get(), GetDangerType())
        .WillByDefault(Return(test_case.general.expected_danger_type));

    scoped_ptr<DownloadTargetInfo> target_info =
        RunDownloadTargetDeterminer(GetPathInDownloadDir(kInitialPath),
                                    item.get());
    VerifyDownloadTarget(test_case.general, target_info.get());
    base::FilePath expected_intermediate_path =
        GetPathInDownloadDir(test_case.expected_intermediate_path);
    if (!expected_intermediate_path.empty())
      EXPECT_EQ(expected_intermediate_path, target_info->intermediate_path);
  }
}

// Test MIME type determination based on the target filename.
TEST_F(DownloadTargetDeterminerTest,
       TargetDeterminer_MIMETypeDetermination) {
  // All test cases run with GetPathInDownloadDir(kInitialPath) as the inital
  // path.
  const base::FilePath::CharType kInitialPath[] =
      FILE_PATH_LITERAL("some_path/bar.txt");

  struct MIMETypeTestCase {
    // General test case settings.
    DownloadTestCase general;

    // Expected MIME type for test case.
    const char* expected_mime_type;
  } kMIMETypeTestCases[] = {
    {
      {
        // 0:
        AUTOMATIC,
        content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
        "http://example.com/foo.png", "image/png",
        FILE_PATH_LITERAL(""),

        FILE_PATH_LITERAL("foo.png"),
        DownloadItem::TARGET_DISPOSITION_OVERWRITE,

        EXPECT_CRDOWNLOAD
      },
      "image/png"
    },
    {
      {
        // 1: Empty MIME type in response.
        AUTOMATIC,
        content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
        "http://example.com/foo.png", "",
        FILE_PATH_LITERAL(""),

        FILE_PATH_LITERAL("foo.png"),
        DownloadItem::TARGET_DISPOSITION_OVERWRITE,

        EXPECT_CRDOWNLOAD
      },
      "image/png"
    },
    {
      {
        // 2: Forced path.
        FORCED,
        content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
        "http://example.com/foo.abc", "",
        FILE_PATH_LITERAL("foo.png"),

        FILE_PATH_LITERAL("foo.png"),
        DownloadItem::TARGET_DISPOSITION_OVERWRITE,

        EXPECT_CRDOWNLOAD
      },
      "image/png"
    },
    {
      {
        // 3: Unknown file type.
        AUTOMATIC,
        content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
        "http://example.com/foo.notarealext", "",
        FILE_PATH_LITERAL(""),

        FILE_PATH_LITERAL("foo.notarealext"),
        DownloadItem::TARGET_DISPOSITION_OVERWRITE,

        EXPECT_CRDOWNLOAD
      },
      ""
    },
    {
      {
        // 4: Unknown file type.
        AUTOMATIC,
        content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
        "http://example.com/foo.notarealext", "image/png",
        FILE_PATH_LITERAL(""),

        FILE_PATH_LITERAL("foo.notarealext"),
        DownloadItem::TARGET_DISPOSITION_OVERWRITE,

        EXPECT_CRDOWNLOAD
      },
      ""
    },
  };

  ON_CALL(*delegate(), GetFileMimeType(
      GetPathInDownloadDir(FILE_PATH_LITERAL("foo.png")), _))
      .WillByDefault(WithArg<1>(
          ScheduleCallback("image/png")));

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kMIMETypeTestCases); ++i) {
    SCOPED_TRACE(testing::Message() << "Running test case " << i);
    const MIMETypeTestCase& test_case = kMIMETypeTestCases[i];
    scoped_ptr<content::MockDownloadItem> item(
        CreateActiveDownloadItem(i, test_case.general));
    scoped_ptr<DownloadTargetInfo> target_info =
        RunDownloadTargetDeterminer(GetPathInDownloadDir(kInitialPath),
                                    item.get());
    EXPECT_EQ(test_case.expected_mime_type, target_info->mime_type);
  }
}

#if defined(ENABLE_PLUGINS)

void DummyGetPluginsCallback(
    const base::Closure& closure,
    const std::vector<content::WebPluginInfo>& plugins) {
  closure.Run();
}

void ForceRefreshOfPlugins() {
#if !defined(OS_WIN)
  // Prevent creation of a utility process for loading plugins. Doing so breaks
  // unit_tests since /proc/self/exe can't be run as a utility process.
  content::RenderProcessHost::SetRunRendererInProcess(true);
#endif
  base::RunLoop run_loop;
  content::PluginService::GetInstance()->GetPlugins(
      base::Bind(&DummyGetPluginsCallback, run_loop.QuitClosure()));
  run_loop.Run();
#if !defined(OS_WIN)
  content::RenderProcessHost::SetRunRendererInProcess(false);
#endif
}

class MockPluginServiceFilter : public content::PluginServiceFilter {
 public:
  MOCK_METHOD1(MockPluginAvailable, bool(const base::FilePath&));

  virtual bool IsPluginAvailable(int render_process_id,
                                 int render_view_id,
                                 const void* context,
                                 const GURL& url,
                                 const GURL& policy_url,
                                 content::WebPluginInfo* plugin) OVERRIDE {
    return MockPluginAvailable(plugin->path);
  }

  virtual bool CanLoadPlugin(int render_process_id,
                             const base::FilePath& path) OVERRIDE {
    return true;
  }
};

class ScopedRegisterInternalPlugin {
 public:
  ScopedRegisterInternalPlugin(content::PluginService* plugin_service,
                               content::WebPluginInfo::PluginType type,
                               const base::FilePath& path,
                               const char* mime_type,
                               const char* extension)
      : plugin_service_(plugin_service),
        plugin_path_(path) {
    content::WebPluginMimeType plugin_mime_type(mime_type,
                                                extension,
                                                "Test file");
    content::WebPluginInfo plugin_info(base::string16(),
                                       path,
                                       base::string16(),
                                       base::string16());
    plugin_info.mime_types.push_back(plugin_mime_type);
    plugin_info.type = type;

    plugin_service->RegisterInternalPlugin(plugin_info, true);
    plugin_service->RefreshPlugins();
    ForceRefreshOfPlugins();
  }

  ~ScopedRegisterInternalPlugin() {
    plugin_service_->UnregisterInternalPlugin(plugin_path_);
    plugin_service_->RefreshPlugins();
    ForceRefreshOfPlugins();
  }

  const base::FilePath& path() { return plugin_path_; }

 private:
  content::PluginService* plugin_service_;
  base::FilePath plugin_path_;
};

// We use a slightly different test fixture for tests that touch plugins. SetUp
// needs to disable plugin discovery and we need to use a
// ShadowingAtExitManager to discard the tainted PluginService. Unfortunately,
// PluginService carries global state.
class DownloadTargetDeterminerTestWithPlugin
    : public DownloadTargetDeterminerTest {
 public:
  DownloadTargetDeterminerTestWithPlugin()
      : old_plugin_service_filter_(NULL) {}

  virtual void SetUp() OVERRIDE {
    content::PluginService* plugin_service =
        content::PluginService::GetInstance();
    plugin_service->Init();
    plugin_service->DisablePluginsDiscoveryForTesting();
    old_plugin_service_filter_ = plugin_service->GetFilter();
    plugin_service->SetFilter(&mock_plugin_filter_);
    DownloadTargetDeterminerTest::SetUp();
  }

  virtual void TearDown() OVERRIDE {
    content::PluginService::GetInstance()->SetFilter(
        old_plugin_service_filter_);
    DownloadTargetDeterminerTest::TearDown();
  }

 protected:
  content::PluginServiceFilter* old_plugin_service_filter_;
  testing::StrictMock<MockPluginServiceFilter> mock_plugin_filter_;
  // The ShadowingAtExitManager destroys the tainted PluginService instance.
  base::ShadowingAtExitManager at_exit_manager_;
};

// Check if secure handling of filetypes is determined correctly for PPAPI
// plugins.
TEST_F(DownloadTargetDeterminerTestWithPlugin,
       TargetDeterminer_CheckForSecureHandling_PPAPI) {
  // All test cases run with GetPathInDownloadDir(kInitialPath) as the inital
  // path.
  const base::FilePath::CharType kInitialPath[] =
      FILE_PATH_LITERAL("some_path/bar.txt");
  const char kTestMIMEType[] = "application/x-example-should-not-exist";

  DownloadTestCase kSecureHandlingTestCase = {
    AUTOMATIC,
    content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
    "http://example.com/foo.fakeext", "",
    FILE_PATH_LITERAL(""),

    FILE_PATH_LITERAL("foo.fakeext"),
    DownloadItem::TARGET_DISPOSITION_OVERWRITE,

    EXPECT_CRDOWNLOAD
  };

  content::PluginService* plugin_service =
      content::PluginService::GetInstance();

  // Verify our test assumptions.
  {
    ForceRefreshOfPlugins();
    std::vector<content::WebPluginInfo> info;
    ASSERT_FALSE(plugin_service->GetPluginInfoArray(
        GURL(), kTestMIMEType, false, &info, NULL));
    ASSERT_EQ(0u, info.size())
        << "Name: " << info[0].name << ", Path: " << info[0].path.value();
  }

  ON_CALL(*delegate(), GetFileMimeType(
      GetPathInDownloadDir(FILE_PATH_LITERAL("foo.fakeext")), _))
      .WillByDefault(WithArg<1>(
          ScheduleCallback(kTestMIMEType)));
  scoped_ptr<content::MockDownloadItem> item(
      CreateActiveDownloadItem(1, kSecureHandlingTestCase));
  scoped_ptr<DownloadTargetInfo> target_info =
      RunDownloadTargetDeterminer(GetPathInDownloadDir(kInitialPath),
                                  item.get());
  EXPECT_FALSE(target_info->is_filetype_handled_safely);

  // Register a PPAPI plugin. This should count as handling the filetype
  // securely.
  ScopedRegisterInternalPlugin ppapi_plugin(
      plugin_service,
      content::WebPluginInfo::PLUGIN_TYPE_PEPPER_OUT_OF_PROCESS,
      test_download_dir().AppendASCII("ppapi"),
      kTestMIMEType,
      "fakeext");
  EXPECT_CALL(mock_plugin_filter_, MockPluginAvailable(ppapi_plugin.path()))
      .WillRepeatedly(Return(true));

  target_info = RunDownloadTargetDeterminer(
      GetPathInDownloadDir(kInitialPath), item.get());
  EXPECT_TRUE(target_info->is_filetype_handled_safely);

  // Try disabling the plugin. Handling should no longer be considered secure.
  EXPECT_CALL(mock_plugin_filter_, MockPluginAvailable(ppapi_plugin.path()))
      .WillRepeatedly(Return(false));
  target_info = RunDownloadTargetDeterminer(
      GetPathInDownloadDir(kInitialPath), item.get());
  EXPECT_FALSE(target_info->is_filetype_handled_safely);

  // Now register an unsandboxed PPAPI plug-in. This plugin should not be
  // considered secure.
  ScopedRegisterInternalPlugin ppapi_unsandboxed_plugin(
      plugin_service,
      content::WebPluginInfo::PLUGIN_TYPE_PEPPER_UNSANDBOXED,
      test_download_dir().AppendASCII("ppapi-nosandbox"),
      kTestMIMEType,
      "fakeext");
  EXPECT_CALL(mock_plugin_filter_,
              MockPluginAvailable(ppapi_unsandboxed_plugin.path()))
      .WillRepeatedly(Return(true));

  target_info = RunDownloadTargetDeterminer(
      GetPathInDownloadDir(kInitialPath), item.get());
  EXPECT_FALSE(target_info->is_filetype_handled_safely);
}

// Check if secure handling of filetypes is determined correctly for NPAPI
// plugins.
TEST_F(DownloadTargetDeterminerTestWithPlugin,
       TargetDeterminer_CheckForSecureHandling_NPAPI) {
  // All test cases run with GetPathInDownloadDir(kInitialPath) as the inital
  // path.
  const base::FilePath::CharType kInitialPath[] =
      FILE_PATH_LITERAL("some_path/bar.txt");
  const char kTestMIMEType[] = "application/x-example-should-not-exist";

  DownloadTestCase kSecureHandlingTestCase = {
    AUTOMATIC,
    content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
    "http://example.com/foo.fakeext", "",
    FILE_PATH_LITERAL(""),

    FILE_PATH_LITERAL("foo.fakeext"),
    DownloadItem::TARGET_DISPOSITION_OVERWRITE,

    EXPECT_CRDOWNLOAD
  };

  content::PluginService* plugin_service =
      content::PluginService::GetInstance();

  // Can't run this test if NPAPI isn't supported.
  if (!plugin_service->NPAPIPluginsSupported())
    return;

  // Verify our test assumptions.
  {
    ForceRefreshOfPlugins();
    std::vector<content::WebPluginInfo> info;
    ASSERT_FALSE(plugin_service->GetPluginInfoArray(
        GURL(), kTestMIMEType, false, &info, NULL));
    ASSERT_EQ(0u, info.size())
        << "Name: " << info[0].name << ", Path: " << info[0].path.value();
  }

  ON_CALL(*delegate(), GetFileMimeType(
      GetPathInDownloadDir(FILE_PATH_LITERAL("foo.fakeext")), _))
      .WillByDefault(WithArg<1>(
          ScheduleCallback(kTestMIMEType)));
  scoped_ptr<content::MockDownloadItem> item(
      CreateActiveDownloadItem(1, kSecureHandlingTestCase));
  scoped_ptr<DownloadTargetInfo> target_info =
      RunDownloadTargetDeterminer(GetPathInDownloadDir(kInitialPath),
                                  item.get());
  EXPECT_FALSE(target_info->is_filetype_handled_safely);

  // Register a NPAPI plugin. This should not count as handling the filetype
  // securely.
  ScopedRegisterInternalPlugin npapi_plugin(
      plugin_service,
      content::WebPluginInfo::PLUGIN_TYPE_NPAPI,
      test_download_dir().AppendASCII("npapi"),
      kTestMIMEType,
      "fakeext");
  EXPECT_CALL(mock_plugin_filter_, MockPluginAvailable(npapi_plugin.path()))
      .WillRepeatedly(Return(true));

  target_info = RunDownloadTargetDeterminer(
      GetPathInDownloadDir(kInitialPath), item.get());
  EXPECT_FALSE(target_info->is_filetype_handled_safely);
}
#endif  // defined(ENABLE_PLUGINS)

}  // namespace
