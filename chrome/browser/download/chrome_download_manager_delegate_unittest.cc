// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/scoped_temp_dir.h"
#include "base/string_util.h"
#include "base/value_conversions.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/download/download_util.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/safe_browsing/download_protection_service.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_switch_utils.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_pref_service.h"
#include "chrome/test/base/testing_profile.h"
#include "content/test/mock_download_item.h"
#include "content/test/mock_download_manager.h"
#include "content/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::DownloadItem;
using safe_browsing::DownloadProtectionService;
using ::testing::AtMost;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::ReturnPointee;
using ::testing::ReturnRef;
using ::testing::ReturnRefOfCopy;
using ::testing::WithArg;
using ::testing::_;

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
ACTION_P(ScheduleCallback, result) {
  MessageLoop::current()->PostTask(FROM_HERE, base::Bind(arg0, result));
}

// Matches a safe_browsing::DownloadProtectionService::DownloadInfo that has
// |url| as the first URL in the |download_url_chain|.
// Example:
//   EXPECT_CALL(Foo(InfoMatchinURL(url)))
MATCHER_P(InfoMatchingURL, url, "DownloadInfo matching URL " + url.spec()) {
  return url == arg.download_url_chain.front();
}

namespace {

// Used with DownloadTestCase. Indicates the type of test case. The expectations
// for the test is set based on the type.
enum TestCaseType {
  SAVE_AS,
  AUTOMATIC,
  FORCED  // Requires that forced_file_path be non-empty.
};

// Used with DownloadTestCase. Indicates whether the a file should be
// overwritten.
enum TestCaseExpectOverwrite {
  EXPECT_OVERWRITE,
  EXPECT_NO_OVERWRITE
};

// Used with DownloadTestCase. Type of intermediate filename to expect.
enum TestCaseExpectIntermediate {
  EXPECT_CRDOWNLOAD,  // Expect path/to/target.crdownload
  EXPECT_UNCONFIRMED  // Expect path/to/Unconfirmed xxx.crdownload
};

// A marker is a file created on the file system to "reserve" a filename. We do
// this for a limited number of cases to prevent in-progress downloads from
// getting overwritten by new downloads.
enum TestCaseExpectMarker {
  EXPECT_NO_MARKER,
  EXPECT_INTERMEDIATE_MARKER
};

// Typical download test case. Used with
// ChromeDownloadManagerDelegateTest::RunTestCase().
struct DownloadTestCase {
  // Type of test.
  TestCaseType                test_type;

  // The |danger_type| value is used to determine the behavior of
  // DownloadProtectionService::IsSupportedDownload(), GetUrlCheckResult() and
  // well as set expectations for GetDangerType() as necessary for flagging the
  // download with as a dangerous download of type |danger_type|.
  content::DownloadDangerType danger_type;

  // Value of GetURL()
  const char*                 url;

  // Value of GetMimeType()
  const char*                 mime_type;

  // Should be non-empty if |test_type| == FORCED. Value of GetForcedFilePath().
  const FilePath::CharType*   forced_file_path;

  // Expected final download path. Specified relative to the test download path.
  const FilePath::CharType*   expected_target_path;

  // Expected target disposition.
  DownloadItem::TargetDisposition expected_disposition;

  // Type of intermediate path to expect.
  TestCaseExpectIntermediate  expected_intermediate;

  // Whether we should overwrite the intermediate path.
  TestCaseExpectOverwrite     expected_overwrite_intermediate;

  // Whether a marker file should be expected.
  TestCaseExpectMarker        expected_marker;
};

// DownloadProtectionService with mock methods. Since the SafeBrowsingService is
// set to NULL, it is not safe to call any non-mocked methods other than
// SetEnabled() and enabled().
class TestDownloadProtectionService
    : public safe_browsing::DownloadProtectionService {
 public:
  TestDownloadProtectionService()
      : safe_browsing::DownloadProtectionService(NULL, NULL) {}
  MOCK_METHOD2(CheckClientDownload,
               void(const DownloadProtectionService::DownloadInfo&,
                    const DownloadProtectionService::CheckDownloadCallback&));
  MOCK_METHOD2(CheckDownloadUrl,
               void(const DownloadProtectionService::DownloadInfo&,
                    const DownloadProtectionService::CheckDownloadCallback&));
  MOCK_CONST_METHOD1(IsSupportedDownload,
                     bool(const DownloadProtectionService::DownloadInfo&));
};

// Subclass of the ChromeDownloadManagerDelegate that uses a mock
// DownloadProtectionService and IsDangerousFile.
class TestChromeDownloadManagerDelegate : public ChromeDownloadManagerDelegate {
 public:
  explicit TestChromeDownloadManagerDelegate(Profile* profile)
      : ChromeDownloadManagerDelegate(profile),
        download_protection_service_(new TestDownloadProtectionService()) {
    download_protection_service_->SetEnabled(true);
  }
  virtual safe_browsing::DownloadProtectionService*
      GetDownloadProtectionService() OVERRIDE {
    return download_protection_service_.get();
  }
  virtual bool IsDangerousFile(const DownloadItem& download,
                               const FilePath& suggested_path,
                               bool visited_referrer_before) OVERRIDE {
    // The implementaion of ChromeDownloadManagerDelegate::IsDangerousFile() is
    // sensitive to a number of external factors (e.g. whether off-store
    // extension installs are allowed, whether a given extension download is
    // approved, whether the user wants files of a given type to be opened
    // automatically etc...). We should test these specifically, but for other
    // tests, we keep the IsDangerousFile() test simple so as not to make the
    // tests flaky.
    return suggested_path.MatchesExtension(FILE_PATH_LITERAL(".jar")) ||
        suggested_path.MatchesExtension(FILE_PATH_LITERAL(".exe"));
  }

  // A TestDownloadProtectionService* is convenient for setting up mocks.
  TestDownloadProtectionService* test_download_protection_service() {
    return download_protection_service_.get();
  }
 private:
  ~TestChromeDownloadManagerDelegate() {}
  scoped_ptr<TestDownloadProtectionService> download_protection_service_;
};

class ChromeDownloadManagerDelegateTest : public ::testing::Test {
 public:
  ChromeDownloadManagerDelegateTest();

  // ::testing::Test
  virtual void SetUp() OVERRIDE;

  // Verifies and clears test expectations for |delegate_| and
  // |download_manager_|.
  void VerifyAndClearExpectations();

  // Creates MockDownloadItem and sets up default expectations.
  content::MockDownloadItem* CreateActiveDownloadItem(int32 id);

  // Sets the AutoOpenBasedOnExtension user preference for |path|.
  void EnableAutoOpenBasedOnExtension(const FilePath& path);

  // Given the relative path |path|, returns the full path under the temporary
  // downloads directory.
  FilePath GetPathInDownloadDir(const FilePath::StringType& path);

  // Run |test_case| using |download_id| as the ID for the download.
  // |download_id| is typically the test case number as well, for convenience.
  void RunTestCase(const DownloadTestCase& test_case, int32 download_id);

  // Set the kDownloadDefaultDirectory user preference to |path|.
  void SetDefaultDownloadPath(const FilePath& path);

  // Set the kDownloadDefaultDirectory managed preference to |path|.
  void SetManagedDownloadPath(const FilePath& path);

  // Set the kPromptForDownload user preference to |prompt|.
  void SetPromptForDownload(bool prompt);

  // Verifies that the intermediate path in |intermediate| is the path that is
  // expected for |target| given the intermediate path type in |expectation|.
  void VerifyIntermediatePath(TestCaseExpectIntermediate expectation,
                              const FilePath& target,
                              const FilePath& intermediate);

  const FilePath& default_download_path() const;
  TestChromeDownloadManagerDelegate* delegate();
  content::MockDownloadManager* download_manager();
  DownloadPrefs* download_prefs();
  void set_clean_marker_files(bool clean);

 private:
  bool clean_marker_files_;
  MessageLoopForUI message_loop_;
  TestingPrefService* pref_service_;
  ScopedTempDir test_download_dir_;
  TestingProfile profile_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;
  scoped_refptr<content::MockDownloadManager> download_manager_;
  scoped_refptr<TestChromeDownloadManagerDelegate> delegate_;
};

ChromeDownloadManagerDelegateTest::ChromeDownloadManagerDelegateTest()
    : clean_marker_files_(true),
      ui_thread_(content::BrowserThread::UI, &message_loop_),
      file_thread_(content::BrowserThread::FILE, &message_loop_),
      download_manager_(new content::MockDownloadManager),
      delegate_(new TestChromeDownloadManagerDelegate(&profile_)) {
  delegate_->SetDownloadManager(download_manager_.get());
  pref_service_ = profile_.GetTestingPrefService();
}

void ChromeDownloadManagerDelegateTest::SetUp() {
  ASSERT_TRUE(test_download_dir_.CreateUniqueTempDir());
  SetDefaultDownloadPath(test_download_dir_.path());
}

void ChromeDownloadManagerDelegateTest::VerifyAndClearExpectations() {
  ::testing::Mock::VerifyAndClearExpectations(delegate_);
  ::testing::Mock::VerifyAndClearExpectations(download_manager_);
}

content::MockDownloadItem*
    ChromeDownloadManagerDelegateTest::CreateActiveDownloadItem(int32 id) {
  content::MockDownloadItem* item =
      new ::testing::NiceMock<content::MockDownloadItem>();
  ON_CALL(*item, GetFullPath())
      .WillByDefault(ReturnRefOfCopy(FilePath()));
  ON_CALL(*item, GetHash())
      .WillByDefault(ReturnRefOfCopy(std::string()));
  ON_CALL(*item, GetReferrerUrl())
      .WillByDefault(ReturnRefOfCopy(GURL()));
  ON_CALL(*item, GetTransitionType())
      .WillByDefault(Return(content::PAGE_TRANSITION_LINK));
  ON_CALL(*item, HasUserGesture())
      .WillByDefault(Return(false));
  ON_CALL(*item, IsDangerous())
      .WillByDefault(Return(false));
  ON_CALL(*item, IsTemporary())
      .WillByDefault(Return(false));
  EXPECT_CALL(*item, GetId())
      .WillRepeatedly(Return(id));
  EXPECT_CALL(*download_manager_, GetActiveDownloadItem(id))
      .WillRepeatedly(Return(item));
  return item;
}

void ChromeDownloadManagerDelegateTest::EnableAutoOpenBasedOnExtension(
    const FilePath& path) {
  EXPECT_TRUE(
      delegate_->download_prefs()->EnableAutoOpenBasedOnExtension(path));
}

FilePath ChromeDownloadManagerDelegateTest::GetPathInDownloadDir(
    const FilePath::StringType& relative_path) {
  if (relative_path.empty())
    return FilePath();
  FilePath full_path(test_download_dir_.path().Append(relative_path));
  return full_path.NormalizePathSeparators();
}

void ChromeDownloadManagerDelegateTest::RunTestCase(
    const DownloadTestCase& test_case,
    int32 download_id) {
  scoped_ptr<content::MockDownloadItem> item(
      CreateActiveDownloadItem(download_id));
  SCOPED_TRACE(
      testing::Message() << "Running test for download id " << download_id);

  // SetUp DownloadItem
  GURL download_url(test_case.url);
  std::vector<GURL> url_chain;
  url_chain.push_back(download_url);
  FilePath forced_file_path(GetPathInDownloadDir(test_case.forced_file_path));
  EXPECT_CALL(*item, GetURL())
      .WillRepeatedly(ReturnRef(download_url));
  EXPECT_CALL(*item, GetUrlChain())
      .WillRepeatedly(ReturnRef(url_chain));
  EXPECT_CALL(*item, GetForcedFilePath())
      .WillRepeatedly(ReturnRef(forced_file_path));
  EXPECT_CALL(*item, GetMimeType())
      .WillRepeatedly(Return(test_case.mime_type));

  // Results of SafeBrowsing URL check.
  DownloadProtectionService::DownloadCheckResult url_check_result =
      (test_case.danger_type == content::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL) ?
          DownloadProtectionService::DANGEROUS :
          DownloadProtectionService::SAFE;
  EXPECT_CALL(*delegate_->test_download_protection_service(),
              CheckDownloadUrl(InfoMatchingURL(download_url), _))
      .WillOnce(WithArg<1>(ScheduleCallback(url_check_result)));

  // Downloads that are flagged as DANGEROUS_URL aren't checked for dangerous
  // content. So we never end up calling IsSupportedDownload for them.
  if (test_case.danger_type != content::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL) {
    bool maybe_dangerous =
        (test_case.danger_type ==
         content::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT);
    EXPECT_CALL(*delegate_->test_download_protection_service(),
                IsSupportedDownload(InfoMatchingURL(download_url)))
        .WillOnce(Return(maybe_dangerous));
  }

  // Expectations for filename determination results.
  FilePath expected_target_path(
      GetPathInDownloadDir(test_case.expected_target_path));
  {
    ::testing::Sequence s1, s2, s3;
    DownloadItem::TargetDisposition initial_disposition =
        (test_case.test_type == SAVE_AS) ?
            DownloadItem::TARGET_DISPOSITION_PROMPT :
            DownloadItem::TARGET_DISPOSITION_OVERWRITE;
    EXPECT_CALL(*item, GetTargetFilePath())
        .InSequence(s1)
        .WillRepeatedly(ReturnRefOfCopy(FilePath()));
    EXPECT_CALL(*item, GetTargetDisposition())
        .InSequence(s2)
        .WillRepeatedly(Return(initial_disposition));
    EXPECT_CALL(*item, GetDangerType())
        .InSequence(s3)
        .WillRepeatedly(Return(content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS));
    EXPECT_CALL(*item, OnTargetPathDetermined(expected_target_path,
                                              test_case.expected_disposition,
                                              test_case.danger_type))
        .InSequence(s1, s2, s3);
    EXPECT_CALL(*item, GetTargetFilePath())
        .InSequence(s1)
        .WillRepeatedly(ReturnRef(expected_target_path));
    EXPECT_CALL(*item, GetTargetDisposition())
        .InSequence(s2)
        .WillRepeatedly(Return(test_case.expected_disposition));
    EXPECT_CALL(*item, GetDangerType())
        .InSequence(s3)
        .WillRepeatedly(Return(test_case.danger_type));
  }

  // RestartDownload() should be called at this point.
  EXPECT_CALL(*download_manager_, RestartDownload(download_id));
  EXPECT_CALL(*download_manager_, LastDownloadPath())
      .WillRepeatedly(Return(FilePath()));

  // Kick off the test.
  EXPECT_FALSE(delegate_->ShouldStartDownload(download_id));
  message_loop_.RunAllPending();

  // Now query the intermediate path.
  EXPECT_CALL(*item, GetDangerType())
      .WillOnce(Return(test_case.danger_type));
  bool ok_to_overwrite = false;
  FilePath intermediate_path =
      delegate_->GetIntermediatePath(*item, &ok_to_overwrite);
  EXPECT_EQ((test_case.expected_overwrite_intermediate == EXPECT_OVERWRITE),
            ok_to_overwrite);
  VerifyIntermediatePath(test_case.expected_intermediate,
                         GetPathInDownloadDir(test_case.expected_target_path),
                         intermediate_path);

  EXPECT_EQ((test_case.expected_marker == EXPECT_INTERMEDIATE_MARKER),
            file_util::PathExists(intermediate_path));
  // Remove the intermediate marker since our mocks won't do anything with it.
  if (clean_marker_files_ &&
      file_util::PathExists(intermediate_path) &&
      !file_util::DirectoryExists(intermediate_path))
    file_util::Delete(intermediate_path, false);
  VerifyAndClearExpectations();
}

void ChromeDownloadManagerDelegateTest::SetDefaultDownloadPath(
    const FilePath& path) {
  pref_service_->SetFilePath(prefs::kDownloadDefaultDirectory, path);
}

void ChromeDownloadManagerDelegateTest::SetManagedDownloadPath(
    const FilePath& path) {
  pref_service_->SetManagedPref(prefs::kDownloadDefaultDirectory,
                                base::CreateFilePathValue(path));
}

void ChromeDownloadManagerDelegateTest::SetPromptForDownload(bool prompt) {
  pref_service_->SetBoolean(prefs::kPromptForDownload, prompt);
}

void ChromeDownloadManagerDelegateTest::VerifyIntermediatePath(
    TestCaseExpectIntermediate expectation,
    const FilePath& target,
    const FilePath& intermediate) {
  if (expectation == EXPECT_CRDOWNLOAD) {
    EXPECT_EQ(download_util::GetCrDownloadPath(target).value(),
              intermediate.value());
  } else {
    // The paths (in English) look like: /path/Unconfirmed xxx.crdownload.
    // Of this, we only check that the path is:
    // 1. Not "/path/target.crdownload",
    // 2. Points to the same directory as the target.
    // 3. Has extension ".crdownload".
    EXPECT_NE(download_util::GetCrDownloadPath(target).value(),
              intermediate.value());
    EXPECT_EQ(target.DirName().value(),
              intermediate.DirName().value());
    EXPECT_TRUE(intermediate.MatchesExtension(
        FILE_PATH_LITERAL(".crdownload")));
  }
}

const FilePath& ChromeDownloadManagerDelegateTest::default_download_path()
    const {
  return test_download_dir_.path();
}

TestChromeDownloadManagerDelegate*
    ChromeDownloadManagerDelegateTest::delegate() {
  return delegate_.get();
}

content::MockDownloadManager*
    ChromeDownloadManagerDelegateTest::download_manager() {
  return download_manager_.get();
}

DownloadPrefs* ChromeDownloadManagerDelegateTest::download_prefs() {
  return delegate_->download_prefs();
}

void ChromeDownloadManagerDelegateTest::set_clean_marker_files(bool clean) {
  clean_marker_files_ = clean;
}

}  // namespace

TEST_F(ChromeDownloadManagerDelegateTest, ShouldStartDownload_Invalid) {
  // Invalid download ID shouldn't do anything.
  EXPECT_CALL(*download_manager(), GetActiveDownloadItem(-1))
      .WillOnce(Return(reinterpret_cast<DownloadItem*>(NULL)));
  EXPECT_FALSE(delegate()->ShouldStartDownload(-1));
}

TEST_F(ChromeDownloadManagerDelegateTest, StartDownload_Basic) {
  const DownloadTestCase kBasicTestCases[] = {
    {
      // 0: Automatic Safe
      AUTOMATIC,
      content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
      "http://example.com/foo.txt", "text/plain",
      FILE_PATH_LITERAL(""),

      FILE_PATH_LITERAL("foo.txt"),
      DownloadItem::TARGET_DISPOSITION_OVERWRITE,

      EXPECT_CRDOWNLOAD,
      EXPECT_OVERWRITE,
      EXPECT_INTERMEDIATE_MARKER
    },

    {
      // 1: Save_As Safe
      SAVE_AS,
      content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
      "http://example.com/foo.txt", "text/plain",
      FILE_PATH_LITERAL(""),

      FILE_PATH_LITERAL("foo.txt"),
      DownloadItem::TARGET_DISPOSITION_PROMPT,

      EXPECT_CRDOWNLOAD,
      EXPECT_OVERWRITE,
      EXPECT_NO_MARKER
    },

    {
      // 2: Automatic Dangerous
      AUTOMATIC,
      content::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT,
      "http://example.com/foo.exe", "",
      FILE_PATH_LITERAL(""),

      FILE_PATH_LITERAL("foo.exe"),
      DownloadItem::TARGET_DISPOSITION_UNIQUIFY,

      EXPECT_UNCONFIRMED,
      EXPECT_NO_OVERWRITE,
      EXPECT_NO_MARKER
    },

    {
      // 3: Save_As Dangerous. For this test case, .jar is considered to be one
      // of the file types supported by safe browsing.
      SAVE_AS,
      content::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT,
      "http://example.com/foo.exe", "",
      FILE_PATH_LITERAL(""),

      FILE_PATH_LITERAL("foo.exe"),
      DownloadItem::TARGET_DISPOSITION_PROMPT,

      EXPECT_UNCONFIRMED,
      EXPECT_NO_OVERWRITE,
      EXPECT_NO_MARKER
    },

    {
      // 4 Forced Safe
      FORCED,
      content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
      "http://example.com/foo.txt", "",
      FILE_PATH_LITERAL("forced-foo.txt"),

      FILE_PATH_LITERAL("forced-foo.txt"),
      DownloadItem::TARGET_DISPOSITION_OVERWRITE,

      EXPECT_CRDOWNLOAD,
      EXPECT_OVERWRITE,
      EXPECT_NO_MARKER
    },

    {
      // 5: Forced Dangerous. As above. .jar is considered to be one of the file
      // types supportred by safe browsing.
      FORCED,
      content::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT,
      "http://example.com/foo.exe", "",
      FILE_PATH_LITERAL("forced-foo.exe"),

      FILE_PATH_LITERAL("forced-foo.exe"),
      DownloadItem::TARGET_DISPOSITION_OVERWRITE,

      EXPECT_UNCONFIRMED,
      EXPECT_NO_OVERWRITE,
      EXPECT_NO_MARKER
    },
  };

  for (unsigned i = 0; i < arraysize(kBasicTestCases); ++i)
    RunTestCase(kBasicTestCases[i], i);
}

// The SafeBrowsing URL check is performed early. Make sure that a download item
// that has been marked as DANGEROUS_URL behaves correctly.
TEST_F(ChromeDownloadManagerDelegateTest, StartDownload_DangerousURL) {
  const DownloadTestCase kDangerousURLTestCases[] = {
    {
      // 0: Automatic Dangerous URL
      AUTOMATIC,
      content::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL,
      "http://phishing.example.com/foo.txt", "",
      FILE_PATH_LITERAL(""),

      FILE_PATH_LITERAL("foo.txt"),
      DownloadItem::TARGET_DISPOSITION_UNIQUIFY,

      EXPECT_UNCONFIRMED,
      EXPECT_NO_OVERWRITE,
      EXPECT_NO_MARKER
    },

    {
      // 1: Save As Dangerous URL
      SAVE_AS,
      content::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL,
      "http://phishing.example.com/foo.txt", "",
      FILE_PATH_LITERAL(""),

      FILE_PATH_LITERAL("foo.txt"),
      DownloadItem::TARGET_DISPOSITION_PROMPT,

      EXPECT_UNCONFIRMED,
      EXPECT_NO_OVERWRITE,
      EXPECT_NO_MARKER
    },

    {
      // 2: Forced Dangerous URL
      FORCED,
      content::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL,
      "http://phishing.example.com/foo.txt", "",
      FILE_PATH_LITERAL("forced-foo.txt"),

      FILE_PATH_LITERAL("forced-foo.txt"),
      DownloadItem::TARGET_DISPOSITION_OVERWRITE,

      EXPECT_UNCONFIRMED,
      EXPECT_NO_OVERWRITE,
      EXPECT_NO_MARKER
    },

    {
      // 3: Automatic Dangerous URL + Dangerous file. Dangerous URL takes
      // precendence.
      AUTOMATIC,
      content::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL,
      "http://phishing.example.com/foo.jar", "",
      FILE_PATH_LITERAL(""),

      FILE_PATH_LITERAL("foo.jar"),
      DownloadItem::TARGET_DISPOSITION_UNIQUIFY,

      EXPECT_UNCONFIRMED,
      EXPECT_NO_OVERWRITE,
      EXPECT_NO_MARKER
    },

    {
      // 4: Save As Dangerous URL + Dangerous file
      SAVE_AS,
      content::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL,
      "http://phishing.example.com/foo.jar", "",
      FILE_PATH_LITERAL(""),

      FILE_PATH_LITERAL("foo.jar"),
      DownloadItem::TARGET_DISPOSITION_PROMPT,

      EXPECT_UNCONFIRMED,
      EXPECT_NO_OVERWRITE,
      EXPECT_NO_MARKER
    },

    {
      // 5: Forced Dangerous URL + Dangerous file
      FORCED,
      content::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL,
      "http://phishing.example.com/foo.jar", "",
      FILE_PATH_LITERAL("forced-foo.jar"),

      FILE_PATH_LITERAL("forced-foo.jar"),
      DownloadItem::TARGET_DISPOSITION_OVERWRITE,

      EXPECT_UNCONFIRMED,
      EXPECT_NO_OVERWRITE,
      EXPECT_NO_MARKER
    },
  };

  for (unsigned i = 0; i < arraysize(kDangerousURLTestCases); ++i)
    RunTestCase(kDangerousURLTestCases[i], i);
}

// These test cases are run with "Prompt for download" user preference set to
// true. Even with the preference set, some of these downloads should not cause
// a prompt to appear.
TEST_F(ChromeDownloadManagerDelegateTest, StartDownload_PromptAlways) {
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

      EXPECT_CRDOWNLOAD,
      EXPECT_OVERWRITE,
      EXPECT_NO_MARKER
    },

    {
      // 1: Dangerous Automatic - Should prompt due to "Prompt for download"
      //    preference setting.
      AUTOMATIC,
      content::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT,
      "http://example.com/foo.exe", "",
      FILE_PATH_LITERAL(""),

      FILE_PATH_LITERAL("foo.exe"),
      DownloadItem::TARGET_DISPOSITION_PROMPT,

      EXPECT_UNCONFIRMED,
      EXPECT_NO_OVERWRITE,
      EXPECT_NO_MARKER
    },

    {
      // 2: Automatic Browser Extension download. - Shouldn't prompt for browser
      //    extension downloads even if "Prompt for download" preference is set.
      AUTOMATIC,
      content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
      "http://example.com/foo.crx",
      extensions::Extension::kMimeType,
      FILE_PATH_LITERAL(""),

      FILE_PATH_LITERAL("foo.crx"),
      DownloadItem::TARGET_DISPOSITION_OVERWRITE,

      EXPECT_CRDOWNLOAD,
      EXPECT_OVERWRITE,
      EXPECT_INTERMEDIATE_MARKER
    },

    {
      // 3: Automatic User Script - Shouldn't prompt for user script downloads
      //    even if "Prompt for download" preference is set.
      AUTOMATIC,
      content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
      "http://example.com/foo.user.js", "",
      FILE_PATH_LITERAL(""),

      FILE_PATH_LITERAL("foo.user.js"),
      DownloadItem::TARGET_DISPOSITION_OVERWRITE,

      EXPECT_CRDOWNLOAD,
      EXPECT_OVERWRITE,
      EXPECT_INTERMEDIATE_MARKER
    },

    {
      // 4: Automatic - The filename extension is marked as one that we will
      //    open automatically. Shouldn't prompt.
      AUTOMATIC,
      content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
      "http://example.com/foo.dummy", "",
      FILE_PATH_LITERAL(""),

      FILE_PATH_LITERAL("foo.dummy"),
      DownloadItem::TARGET_DISPOSITION_OVERWRITE,

      EXPECT_CRDOWNLOAD,
      EXPECT_OVERWRITE,
      EXPECT_INTERMEDIATE_MARKER
    },
  };

  SetPromptForDownload(true);
  EnableAutoOpenBasedOnExtension(FilePath(FILE_PATH_LITERAL("dummy.dummy")));
  for (unsigned i = 0; i < arraysize(kPromptingTestCases); ++i)
    RunTestCase(kPromptingTestCases[i], i);
}

// If the download path is managed, then we don't show any prompts.
// Note that if the download path is managed, then PromptForDownload() is false.
TEST_F(ChromeDownloadManagerDelegateTest, StartDownload_ManagedPath) {
  const DownloadTestCase kManagedPathTestCases[] = {
    {
      // 0: Automatic Safe
      AUTOMATIC,
      content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
      "http://example.com/foo.txt", "text/plain",
      FILE_PATH_LITERAL(""),

      FILE_PATH_LITERAL("foo.txt"),
      DownloadItem::TARGET_DISPOSITION_OVERWRITE,

      EXPECT_CRDOWNLOAD,
      EXPECT_OVERWRITE,
      EXPECT_INTERMEDIATE_MARKER
    },

    {
      // 1: Save_As Safe
      SAVE_AS,
      content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
      "http://example.com/foo.txt", "text/plain",
      FILE_PATH_LITERAL(""),

      FILE_PATH_LITERAL("foo.txt"),
      DownloadItem::TARGET_DISPOSITION_OVERWRITE,

      EXPECT_CRDOWNLOAD,
      EXPECT_OVERWRITE,
      EXPECT_INTERMEDIATE_MARKER
    },
  };

  SetManagedDownloadPath(default_download_path());
  ASSERT_TRUE(download_prefs()->IsDownloadPathManaged());
  for (unsigned i = 0; i < arraysize(kManagedPathTestCases); ++i)
    RunTestCase(kManagedPathTestCases[i], i);
}

// If the intermediate file already exists (for safe downloads), or if the
// target file already exists, then the suggested target path should be
// uniquified.
TEST_F(ChromeDownloadManagerDelegateTest, StartDownload_ConflictingFiles) {
  // Create some target files and an intermediate file for use with the test
  // cases that follow:
  ASSERT_TRUE(file_util::IsDirectoryEmpty(default_download_path()));
  ASSERT_EQ(0, file_util::WriteFile(
      GetPathInDownloadDir(FILE_PATH_LITERAL("exists.txt")), "", 0));
  ASSERT_EQ(0, file_util::WriteFile(
      GetPathInDownloadDir(FILE_PATH_LITERAL("exists.jar")), "", 0));
  ASSERT_EQ(0, file_util::WriteFile(
      GetPathInDownloadDir(FILE_PATH_LITERAL("exists.exe")), "", 0));
  ASSERT_EQ(0, file_util::WriteFile(
      GetPathInDownloadDir(FILE_PATH_LITERAL("exists.png.crdownload")), "", 0));

  const DownloadTestCase kConflictingFilesTestCases[] = {
    {
      // 0: Automatic Safe
      AUTOMATIC,
      content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
      "http://example.com/exists.txt", "",
      FILE_PATH_LITERAL(""),

      FILE_PATH_LITERAL("exists (1).txt"),
      DownloadItem::TARGET_DISPOSITION_OVERWRITE,

      EXPECT_CRDOWNLOAD,
      EXPECT_OVERWRITE,
      EXPECT_INTERMEDIATE_MARKER
    },

    {
      // 1: Save_As Safe
      SAVE_AS,
      content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
      "http://example.com/exists.txt", "",
      FILE_PATH_LITERAL(""),

      FILE_PATH_LITERAL("exists (1).txt"),
      DownloadItem::TARGET_DISPOSITION_PROMPT,

      EXPECT_CRDOWNLOAD,
      EXPECT_OVERWRITE,
      EXPECT_NO_MARKER
    },

    {
      // 2: Automatic Dangerous
      AUTOMATIC,
      content::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE,
      "http://example.com/exists.jar", "",
      FILE_PATH_LITERAL(""),

      FILE_PATH_LITERAL("exists.jar"),
      DownloadItem::TARGET_DISPOSITION_UNIQUIFY,

      EXPECT_UNCONFIRMED,
      EXPECT_NO_OVERWRITE,
      EXPECT_NO_MARKER
    },

    {
      // 3: Save_As Dangerous
      SAVE_AS,
      content::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT,
      "http://example.com/exists.exe", "",
      FILE_PATH_LITERAL(""),

      FILE_PATH_LITERAL("exists (1).exe"),
      DownloadItem::TARGET_DISPOSITION_PROMPT,

      EXPECT_UNCONFIRMED,
      EXPECT_NO_OVERWRITE,
      EXPECT_NO_MARKER
    },

    {
      // 4: Automatic Maybe_Dangerous
      AUTOMATIC,
      content::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT,
      "http://example.com/exists.exe", "",
      FILE_PATH_LITERAL(""),

      FILE_PATH_LITERAL("exists.exe"),
      DownloadItem::TARGET_DISPOSITION_UNIQUIFY,

      EXPECT_UNCONFIRMED,
      EXPECT_NO_OVERWRITE,
      EXPECT_NO_MARKER
    },

    {
      // 5: Save_As Maybe_Dangerous
      SAVE_AS,
      content::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT,
      "http://example.com/exists.exe", "",
      FILE_PATH_LITERAL(""),

      FILE_PATH_LITERAL("exists (1).exe"),
      DownloadItem::TARGET_DISPOSITION_PROMPT,

      EXPECT_UNCONFIRMED,
      EXPECT_NO_OVERWRITE,
      EXPECT_NO_MARKER
    },

    {
      // 6: Automatic Safe - Intermediate path exists
      AUTOMATIC,
      content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
      "http://example.com/exists.png", "",
      FILE_PATH_LITERAL(""),

      FILE_PATH_LITERAL("exists (1).png"),
      DownloadItem::TARGET_DISPOSITION_OVERWRITE,

      EXPECT_CRDOWNLOAD,
      EXPECT_OVERWRITE,
      EXPECT_INTERMEDIATE_MARKER
    },

    {
      // 7: Save_As Safe - Intermediate path exists
      SAVE_AS,
      content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
      "http://example.com/exists.png", "",
      FILE_PATH_LITERAL(""),

      FILE_PATH_LITERAL("exists (1).png"),
      DownloadItem::TARGET_DISPOSITION_PROMPT,

      EXPECT_CRDOWNLOAD,
      EXPECT_OVERWRITE,
      EXPECT_NO_MARKER
    },
  };

  for (unsigned i = 0; i < arraysize(kConflictingFilesTestCases); ++i)
    RunTestCase(kConflictingFilesTestCases[i], i);
}

// Check that multiple downloads that are in-progress at the same time don't
// trample each other. This is only done in a few limited cases currently.
TEST_F(ChromeDownloadManagerDelegateTest, StartDownload_ConflictingDownloads) {
  const DownloadTestCase kConflictingDownloadsTestCases[] = {
    {
      // 0: Initial download.
      AUTOMATIC,
      content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
      "http://example.com/exists.txt", "",
      FILE_PATH_LITERAL(""),

      FILE_PATH_LITERAL("exists.txt"),
      DownloadItem::TARGET_DISPOSITION_OVERWRITE,

      EXPECT_CRDOWNLOAD,
      EXPECT_OVERWRITE,
      EXPECT_INTERMEDIATE_MARKER
    },

    {
      // 1: Should uniquify around the previous download.
      AUTOMATIC,
      content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
      "http://example.com/exists.txt", "",
      FILE_PATH_LITERAL(""),

      FILE_PATH_LITERAL("exists (1).txt"),
      DownloadItem::TARGET_DISPOSITION_OVERWRITE,

      EXPECT_CRDOWNLOAD,
      EXPECT_OVERWRITE,
      EXPECT_INTERMEDIATE_MARKER
    },

    {
      // 2: Uniquifies around both previous downloads.
      AUTOMATIC,
      content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
      "http://example.com/exists.txt", "",
      FILE_PATH_LITERAL(""),

      FILE_PATH_LITERAL("exists (2).txt"),
      DownloadItem::TARGET_DISPOSITION_OVERWRITE,

      EXPECT_CRDOWNLOAD,
      EXPECT_OVERWRITE,
      EXPECT_INTERMEDIATE_MARKER
    },
  };

  set_clean_marker_files(false);
  for (unsigned i = 0; i < arraysize(kConflictingDownloadsTestCases); ++i)
    RunTestCase(kConflictingDownloadsTestCases[i], i);
}

// TODO(asanka): Add more tests.
// * Default download path is not writable.
// * Download path doesn't exist.
// * IsDangerousFile().
// * Filename generation.
