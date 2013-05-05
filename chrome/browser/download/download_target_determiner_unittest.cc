// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/message_loop.h"
#include "base/observer_list.h"
#include "base/prefs/pref_service.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/string_util.h"
#include "base/value_conversions.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_extensions.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/download/download_target_determiner.h"
#include "chrome/browser/download/download_util.h"
#include "chrome/browser/history/history_service.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_pref_service_syncable.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/test/mock_download_item.h"
#include "content/public/test/test_browser_thread.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

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
  EXPECT_CRDOWNLOAD,  // Expect path/to/target.crdownload.
  EXPECT_UNCONFIRMED, // Expect path/to/Unconfirmed xxx.crdownload.
  EXPECT_LOCAL_PATH,  // Expect target path.
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

  // Expected virtual path. Specified relative to the virtual download path. If
  // empty, assumed to be the same as |expected_local_path|.
  const base::FilePath::CharType* expected_virtual_path;

  // Expected local path. Specified relative to the test download path.
  const base::FilePath::CharType* expected_local_path;

  // Expected target disposition. If this is TARGET_DISPOSITION_PROMPT, then the
  // test run will expect ChromeDownloadManagerDelegate to prompt the user for a
  // download location.
  DownloadItem::TargetDisposition expected_disposition;

  // Type of intermediate path to expect.
  TestCaseExpectIntermediate expected_intermediate;
};

class MockDownloadTargetDeterminerDelegate :
      public DownloadTargetDeterminerDelegate {
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
  DownloadTargetDeterminerTest();

  // ::testing::Test
  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

  // Creates MockDownloadItem and sets up default expectations.
  content::MockDownloadItem* CreateActiveDownloadItem(
      int32 id,
      const DownloadTestCase& test_case);

  // Sets the AutoOpenBasedOnExtension user preference for |path|.
  void EnableAutoOpenBasedOnExtension(const base::FilePath& path);

  // Set the kDownloadDefaultDirectory user preference to |path|.
  void SetDefaultDownloadPath(const base::FilePath& path);

  // Set the kDownloadDefaultDirectory managed preference to |path|.
  void SetManagedDownloadPath(const base::FilePath& path);

  // Set the kPromptForDownload user preference to |prompt|.
  void SetPromptForDownload(bool prompt);

  // Given the relative path |path|, returns the full path under the temporary
  // downloads directory.
  base::FilePath GetPathInDownloadDir(const base::FilePath::StringType& path);

  // Run |test_case| using |item|.
  void RunTestCase(const DownloadTestCase& test_case,
                   content::MockDownloadItem* item);

  // Run through |test_case_count| tests in |test_cases|. A new MockDownloadItem
  // will be created for each test case and destroyed when the test case is
  // complete.
  void RunTestCasesWithActiveItem(const DownloadTestCase test_cases[],
                                  size_t test_case_count);

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

  void set_last_selected_directory(const base::FilePath& path) {
    last_selected_directory_ = path;
  }

 private:
  // Verifies that |target_path|, |disposition|, |expected_danger_type| and
  // |intermediate_path| matches the expectations of |test_case|. Posts
  // |closure| to the current message loop when done.
  void DownloadTargetVerifier(const base::Closure& closure,
                              const DownloadTestCase& test_case,
                              const base::FilePath& local_path,
                              DownloadItem::TargetDisposition disposition,
                              content::DownloadDangerType danger_type,
                              const base::FilePath& intermediate_path);

  scoped_ptr<DownloadPrefs> download_prefs_;
  ::testing::NiceMock<MockDownloadTargetDeterminerDelegate> delegate_;
  NullWebContentsDelegate web_contents_delegate_;
  base::ScopedTempDir test_download_dir_;
  base::FilePath test_virtual_dir_;
  base::FilePath last_selected_directory_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;
};

DownloadTargetDeterminerTest::DownloadTargetDeterminerTest()
    : ChromeRenderViewHostTestHarness(),
      ui_thread_(content::BrowserThread::UI, &message_loop_),
      file_thread_(content::BrowserThread::FILE, &message_loop_) {
}

void DownloadTargetDeterminerTest::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();
  CHECK(profile());
  download_prefs_.reset(new DownloadPrefs(profile()));
  web_contents()->SetDelegate(&web_contents_delegate_);
  ASSERT_TRUE(test_download_dir_.CreateUniqueTempDir());
  test_virtual_dir_ = test_download_dir().Append(FILE_PATH_LITERAL("virtual"));
  SetDefaultDownloadPath(test_download_dir());
  delegate_.SetupDefaults();
}

void DownloadTargetDeterminerTest::TearDown() {
  download_prefs_.reset();
  message_loop_.RunUntilIdle();
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
  ON_CALL(*item, IsInProgress())
      .WillByDefault(Return(true));
  ON_CALL(*item, IsTemporary())
      .WillByDefault(Return(false));
  return item;
}

void DownloadTargetDeterminerTest::EnableAutoOpenBasedOnExtension(
    const base::FilePath& path) {
  EXPECT_TRUE(download_prefs_->EnableAutoOpenBasedOnExtension(path));
}

void DownloadTargetDeterminerTest::SetDefaultDownloadPath(
    const base::FilePath& path) {
  profile()->GetTestingPrefService()->
      SetFilePath(prefs::kDownloadDefaultDirectory, path);
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
    content::MockDownloadItem* item) {
  // Kick off the test.
  base::WeakPtrFactory<DownloadTargetDeterminerTest> factory(this);
  base::RunLoop run_loop;
  DownloadTargetDeterminer::Start(
      item, download_prefs_.get(), last_selected_directory_, delegate(),
      base::Bind(&DownloadTargetDeterminerTest::DownloadTargetVerifier,
                 factory.GetWeakPtr(), run_loop.QuitClosure(), test_case));
  run_loop.Run();
  ::testing::Mock::VerifyAndClearExpectations(delegate());
}

void DownloadTargetDeterminerTest::RunTestCasesWithActiveItem(
    const DownloadTestCase test_cases[],
    size_t test_case_count) {
  for (size_t i = 0; i < test_case_count; ++i) {
    scoped_ptr<content::MockDownloadItem> item(
        CreateActiveDownloadItem(i, test_cases[i]));
    SCOPED_TRACE(testing::Message() << "Running test case " << i);
    RunTestCase(test_cases[i], item.get());
  }
}

void DownloadTargetDeterminerTest::DownloadTargetVerifier(
    const base::Closure& closure,
    const DownloadTestCase& test_case,
    const base::FilePath& local_path,
    DownloadItem::TargetDisposition disposition,
    content::DownloadDangerType danger_type,
    const base::FilePath& intermediate_path) {
  base::FilePath expected_local_path(
      GetPathInDownloadDir(test_case.expected_local_path));
  EXPECT_EQ(expected_local_path.value(), local_path.value());
  EXPECT_EQ(test_case.expected_disposition, disposition);
  EXPECT_EQ(test_case.expected_danger_type, danger_type);

  switch (test_case.expected_intermediate) {
    case EXPECT_CRDOWNLOAD:
      EXPECT_EQ(download_util::GetCrDownloadPath(local_path).value(),
                intermediate_path.value());
      break;

    case EXPECT_UNCONFIRMED:
      // The paths (in English) look like: /path/Unconfirmed xxx.crdownload.
      // Of this, we only check that the path is:
      // 1. Not "/path/target.crdownload",
      // 2. Points to the same directory as the target.
      // 3. Has extension ".crdownload".
      // 4. Basename starts with "Unconfirmed ".
      EXPECT_NE(download_util::GetCrDownloadPath(expected_local_path).value(),
                intermediate_path.value());
      EXPECT_EQ(expected_local_path.DirName().value(),
                intermediate_path.DirName().value());
      EXPECT_TRUE(intermediate_path.MatchesExtension(
          FILE_PATH_LITERAL(".crdownload")));
      EXPECT_EQ(0u, intermediate_path.BaseName().value().find(
          FILE_PATH_LITERAL("Unconfirmed ")));
      break;

    case EXPECT_LOCAL_PATH:
      EXPECT_EQ(expected_local_path.value(), intermediate_path.value());
      break;
  }
  base::MessageLoop::current()->PostTask(FROM_HERE, closure);
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

      FILE_PATH_LITERAL(""),
      FILE_PATH_LITERAL("foo.txt"),
      DownloadItem::TARGET_DISPOSITION_PROMPT,

      EXPECT_CRDOWNLOAD
    },

    {
      // 2: Automatic Dangerous
      AUTOMATIC,
      content::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE,
      "http://example.com/foo.html", "",
      FILE_PATH_LITERAL(""),

      FILE_PATH_LITERAL(""),
      FILE_PATH_LITERAL("foo.html"),
      DownloadItem::TARGET_DISPOSITION_OVERWRITE,

      EXPECT_UNCONFIRMED
    },

    {
      // 3: Forced Safe
      FORCED,
      content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
      "http://example.com/foo.txt", "",
      FILE_PATH_LITERAL("forced-foo.txt"),

      FILE_PATH_LITERAL(""),
      FILE_PATH_LITERAL("forced-foo.txt"),
      DownloadItem::TARGET_DISPOSITION_OVERWRITE,

      EXPECT_LOCAL_PATH
    },
  };

  // The test assumes that .html files have a danger level of
  // AllowOnUserGesture.
  ASSERT_EQ(download_util::AllowOnUserGesture,
            download_util::GetFileDangerLevel(
                base::FilePath(FILE_PATH_LITERAL("foo.html"))));
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

      FILE_PATH_LITERAL(""),
      FILE_PATH_LITERAL("forced-foo.txt"),
      DownloadItem::TARGET_DISPOSITION_OVERWRITE,

      EXPECT_UNCONFIRMED
    },

    {
      // 3: Automatic Dangerous URL + Dangerous file. Dangerous URL takes
      // precendence.
      AUTOMATIC,
      content::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL,
      "http://phishing.example.com/foo.html", "",
      FILE_PATH_LITERAL(""),

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

      FILE_PATH_LITERAL(""),
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

      FILE_PATH_LITERAL(""),
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

      FILE_PATH_LITERAL("virtual/foo/foo.txt"),
      FILE_PATH_LITERAL("bar.txt"),
      DownloadItem::TARGET_DISPOSITION_PROMPT,

      EXPECT_LOCAL_PATH
    },
  };

  {
    SCOPED_TRACE(testing::Message()
                 << "Running with empty last_selected_directory");
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
    set_last_selected_directory(test_download_dir().AppendASCII("foo"));
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
    set_last_selected_directory(last_selected_dir);
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
  SetDefaultDownloadPath(test_virtual_dir());

  {
    SCOPED_TRACE(testing::Message() << "Automatic Safe Download");
    const DownloadTestCase kAutomaticDownloadToVirtualDir = {
      AUTOMATIC,
      content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
      "http://example.com/foo.txt", "text/plain",
      FILE_PATH_LITERAL(""),

      // Downloaded to default virtual directory.
      FILE_PATH_LITERAL("virtual/foo.txt"),
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

      // The response to the download prompt is to choose the 'prompted.txt'
      // virtual path.
      FILE_PATH_LITERAL("virtual/prompted.txt"),
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

      // Response to the 'Save As' is to choose the local path for 'foo-x.txt'.
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

      // Forced paths should be left as-is.
      FILE_PATH_LITERAL(""),
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

      FILE_PATH_LITERAL("foo.txt"),
      FILE_PATH_LITERAL(""),
      DownloadItem::TARGET_DISPOSITION_OVERWRITE,

      EXPECT_LOCAL_PATH
    },

    {
      SAVE_AS,
      content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
      "http://example.com/foo.txt", "text/plain",
      FILE_PATH_LITERAL(""),

      FILE_PATH_LITERAL("foo.txt"),
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
    EXPECT_CALL(*item.get(), IsInProgress())
        .WillRepeatedly(Return(false));
    // Even though one is a SAVE_AS download, no prompt will be displayed to
    // the user because the download is inactive.
    EXPECT_CALL(*delegate(), PromptUserForDownloadPath(_, _, _))
        .Times(0);
    RunTestCase(kInactiveTestCases[i], item.get());
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
      FILE_PATH_LITERAL(""),
      DownloadItem::TARGET_DISPOSITION_OVERWRITE,

      EXPECT_LOCAL_PATH
    },
  };

  base::FilePath expected_virtual_path(
      GetPathInDownloadDir(FILE_PATH_LITERAL("virtual/foo.txt")));
  // The default download directory is the virtual path.
  SetDefaultDownloadPath(test_virtual_dir());
  // Simulate failed call to DetermineLocalPath.
  EXPECT_CALL(*delegate(), DetermineLocalPath(
      _, GetPathInDownloadDir(FILE_PATH_LITERAL("virtual/foo.txt")), _))
      .WillOnce(WithArg<2>(ScheduleCallback(base::FilePath())));
  RunTestCasesWithActiveItem(kLocalPathFailedCases,
                             arraysize(kLocalPathFailedCases));
}

// Downloads that have a danger level of AllowOnUserGesture should be marked as
// safe depending on whether there was a user gesture associated with the
// download and whether the referrer was visited prior to today.
TEST_F(DownloadTargetDeterminerTest, TargetDeterminer_VisitedReferrer) {
  const DownloadTestCase kVisitedReferrerCases[] = {
    // http://visited.example.com/ is added to the history as a visit that
    // happened prior to today.
    {
      // 0: Safe download due to visiting referrer before.
      AUTOMATIC,
      content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
      "http://visited.example.com/foo.html", "text/html",
      FILE_PATH_LITERAL(""),

      FILE_PATH_LITERAL(""),
      FILE_PATH_LITERAL("foo.html"),
      DownloadItem::TARGET_DISPOSITION_OVERWRITE,

      EXPECT_CRDOWNLOAD
    },

    {
      // 1: Dangerous due to not having visited referrer before.
      AUTOMATIC,
      content::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE,
      "http://not-visited.example.com/foo.html", "text/html",
      FILE_PATH_LITERAL(""),

      FILE_PATH_LITERAL(""),
      FILE_PATH_LITERAL("foo.html"),
      DownloadItem::TARGET_DISPOSITION_OVERWRITE,

      EXPECT_UNCONFIRMED
    },

    {
      // 2: Safe because the user is being prompted.
      SAVE_AS,
      content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
      "http://not-visited.example.com/foo.html", "text/html",
      FILE_PATH_LITERAL(""),

      FILE_PATH_LITERAL(""),
      FILE_PATH_LITERAL("foo.html"),
      DownloadItem::TARGET_DISPOSITION_PROMPT,

      EXPECT_CRDOWNLOAD
    },

    {
      // 3: Safe because of forced path.
      FORCED,
      content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
      "http://not-visited.example.com/foo.html", "text/html",
      FILE_PATH_LITERAL("foo.html"),

      FILE_PATH_LITERAL(""),
      FILE_PATH_LITERAL("foo.html"),
      DownloadItem::TARGET_DISPOSITION_OVERWRITE,

      EXPECT_LOCAL_PATH
    },
  };

  // This test assumes that the danger level of .html files is
  // AllowOnUserGesture.
  ASSERT_EQ(download_util::AllowOnUserGesture,
            download_util::GetFileDangerLevel(
                base::FilePath(FILE_PATH_LITERAL("foo.html"))));

  // First the history service must exist.
  profile()->CreateHistoryService(false, false);

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

      FILE_PATH_LITERAL(""),
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

      FILE_PATH_LITERAL(""),
      FILE_PATH_LITERAL("overridden/foo.txt"),
      DownloadItem::TARGET_DISPOSITION_PROMPT,

      EXPECT_CRDOWNLOAD
    },

    {
      // 2: Automatic Dangerous
      AUTOMATIC,
      content::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE,
      "http://example.com/foo.html", "",
      FILE_PATH_LITERAL(""),

      FILE_PATH_LITERAL(""),
      FILE_PATH_LITERAL("overridden/foo.html"),
      DownloadItem::TARGET_DISPOSITION_OVERWRITE,

      EXPECT_UNCONFIRMED
    },

    {
      // 3: Forced Safe
      FORCED,
      content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
      "http://example.com/foo.txt", "",
      FILE_PATH_LITERAL("forced-foo.txt"),

      FILE_PATH_LITERAL(""),
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
      "http://example.com/foo.html.remove", "text/plain",
      FILE_PATH_LITERAL(""),

      FILE_PATH_LITERAL(""),
      FILE_PATH_LITERAL("overridden/foo.html"),
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

  RunTestCase(test_case, item.get());

  // Second case: An extension sets the conflict_action to PROMPT.
  EXPECT_CALL(*delegate(), NotifyExtensions(_, _, _))
      .WillOnce(WithArg<2>(
          ScheduleCallback2(overridden_path,
                            DownloadPathReservationTracker::PROMPT)));
  EXPECT_CALL(*delegate(), ReserveVirtualPath(
      _, full_overridden_path, true, DownloadPathReservationTracker::PROMPT, _))
      .WillOnce(WithArg<4>(
          ScheduleCallback2(full_overridden_path, true)));
  RunTestCase(test_case, item.get());
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

  // The last selected directory is this one. Since the test case is a SAVE_AS
  // download, it should use this directory for the generated path.
  set_last_selected_directory(GetPathInDownloadDir(
      FILE_PATH_LITERAL("last_selected")));

  EXPECT_CALL(*delegate(), NotifyExtensions(_, _, _))
      .WillOnce(WithArg<2>(
          ScheduleCallback2(overridden_path,
                            DownloadPathReservationTracker::UNIQUIFY)));
  EXPECT_CALL(*delegate(),
              PromptUserForDownloadPath(_, full_overridden_path, _))
      .WillOnce(WithArg<2>(
          ScheduleCallback(full_overridden_path)));
  RunTestCase(test_case, item.get());
}
}  // namespace
