// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/observer_list.h"
#include "base/scoped_temp_dir.h"
#include "base/stl_util.h"
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
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_pref_service.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/test/mock_download_item.h"
#include "content/public/test/mock_download_manager.h"
#include "content/public/test/test_browser_thread.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
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

namespace {

class MockWebContentsDelegate : public content::WebContentsDelegate {
 public:
  virtual ~MockWebContentsDelegate() {}
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

// Typical download test case. Used with
// ChromeDownloadManagerDelegateTest::RunTestCase().
struct DownloadTestCase {
  // Type of test.
  TestCaseType                test_type;

  // The |danger_type| is the expected danger type for the download as
  // determined by CDMD. This value is also used to determine the behavior of
  // DownloadProtectionService::IsSupportedDownload(), CDMD::CheckDownloadUrl()
  // as necessary for flagging the download with as a dangerous download of type
  // |danger_type|.
  content::DownloadDangerType danger_type;

  // Value of DownloadItem::GetURL()
  const char*                 url;

  // Value of DownloadItem::GetMimeType()
  const char*                 mime_type;

  // Should be non-empty if |test_type| == FORCED. Value of GetForcedFilePath().
  const FilePath::CharType*   forced_file_path;

  // Expected final download path. Specified relative to the test download path.
  // If the user is presented with a file chooser, this path will also be the
  // response sent back from the file chooser.
  const FilePath::CharType*   expected_target_path;

  // The path to expect as the suggested path if the user will be prompted for a
  // download path.
  const FilePath::CharType*   expected_prompt_path;

  // Expected target disposition. If this is TARGET_DISPOSITION_PROMPT, then the
  // test run will expect ChromeDownloadManagerDelegate to prompt the user for a
  // download location.
  DownloadItem::TargetDisposition expected_disposition;

  // Type of intermediate path to expect.
  TestCaseExpectIntermediate  expected_intermediate;
};

#if defined(ENABLE_SAFE_BROWSING)
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
#endif

// Subclass of the ChromeDownloadManagerDelegate that uses a mock
// DownloadProtectionService and IsDangerousFile.
class TestChromeDownloadManagerDelegate : public ChromeDownloadManagerDelegate {
 public:
  explicit TestChromeDownloadManagerDelegate(Profile* profile)
      : ChromeDownloadManagerDelegate(profile) {
#if defined(ENABLE_SAFE_BROWSING)
    download_protection_service_.reset(new TestDownloadProtectionService());
    download_protection_service_->SetEnabled(true);
#endif
  }

  virtual safe_browsing::DownloadProtectionService*
      GetDownloadProtectionService() OVERRIDE {
#if defined(ENABLE_SAFE_BROWSING)
    return download_protection_service_.get();
#else
    return NULL;
#endif
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

  virtual void GetReservedPath(
      content::DownloadItem& download,
      const FilePath& target_path,
      const FilePath& default_download_path,
      bool should_uniquify_path,
      const DownloadPathReservationTracker::ReservedPathCallback& callback)
      OVERRIDE {
    // Pretend the path reservation succeeded without any change to
    // |target_path|.
    MessageLoop::current()->PostTask(FROM_HERE,
                                     base::Bind(callback, target_path, true));
  }

  // During tests, we want to mock the behavior of this method.
  MOCK_METHOD3(ChooseDownloadPath,
               void(content::DownloadItem*,
                    const FilePath&,
                    const FileSelectedCallback&));

#if defined(ENABLE_SAFE_BROWSING)
  // A TestDownloadProtectionService* is convenient for setting up mocks.
  TestDownloadProtectionService* test_download_protection_service() {
    return download_protection_service_.get();
  }
#endif

 private:
  ~TestChromeDownloadManagerDelegate() {}

#if defined(ENABLE_SAFE_BROWSING)
  scoped_ptr<TestDownloadProtectionService> download_protection_service_;
#endif
};

class ChromeDownloadManagerDelegateTest :
    public ChromeRenderViewHostTestHarness {
 public:
  ChromeDownloadManagerDelegateTest();

  // ::testing::Test
  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

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

  // Run |test_case| using |item|.
  void RunTestCaseWithDownloadItem(const DownloadTestCase& test_case,
                                   content::MockDownloadItem* item);

  // Run through |test_case_count| tests in |test_cases|. A new MockDownloadItem
  // will be created for each test case and destroyed when the test case is
  // complete.
  void RunTestCases(const DownloadTestCase test_cases[],
                    size_t test_case_count);

  // Set the kDownloadDefaultDirectory user preference to |path|.
  void SetDefaultDownloadPath(const FilePath& path);

  // Set the kDownloadDefaultDirectory managed preference to |path|.
  void SetManagedDownloadPath(const FilePath& path);

  // Set the kPromptForDownload user preference to |prompt|.
  void SetPromptForDownload(bool prompt);

  const FilePath& default_download_path() const;
  TestChromeDownloadManagerDelegate* delegate();
  content::MockDownloadManager* download_manager();
  DownloadPrefs* download_prefs();

 private:
  // Verifies that |target_path|, |disposition|, |danger_type| and
  // |intermediate_path| matches the expectations of |test_case|.
  void DownloadTargetVerifier(const DownloadTestCase* test_case,
                              const FilePath& target_path,
                              DownloadItem::TargetDisposition disposition,
                              content::DownloadDangerType danger_type,
                              const FilePath& intermediate_path);

  TestingPrefService* pref_service_;
  ScopedTempDir test_download_dir_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;
  scoped_refptr<content::MockDownloadManager> download_manager_;
  scoped_refptr<TestChromeDownloadManagerDelegate> delegate_;
  MockWebContentsDelegate web_contents_delegate_;
};

ChromeDownloadManagerDelegateTest::ChromeDownloadManagerDelegateTest()
    : ChromeRenderViewHostTestHarness(),
      ui_thread_(content::BrowserThread::UI, &message_loop_),
      file_thread_(content::BrowserThread::FILE, &message_loop_),
      download_manager_(new content::MockDownloadManager) {
}

void ChromeDownloadManagerDelegateTest::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();

  CHECK(profile());
  delegate_ = new TestChromeDownloadManagerDelegate(profile());
  delegate_->SetDownloadManager(download_manager_.get());
  pref_service_ = profile()->GetTestingPrefService();
  contents()->SetDelegate(&web_contents_delegate_);

  ASSERT_TRUE(test_download_dir_.CreateUniqueTempDir());
  SetDefaultDownloadPath(test_download_dir_.path());
}

void ChromeDownloadManagerDelegateTest::TearDown() {
  message_loop_.RunAllPending();
  delegate_->Shutdown();
  ChromeRenderViewHostTestHarness::TearDown();
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
  ON_CALL(*item, GetWebContents())
      .WillByDefault(Return(contents()));
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

void ChromeDownloadManagerDelegateTest::RunTestCaseWithDownloadItem(
    const DownloadTestCase& test_case,
    content::MockDownloadItem* item) {
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

#if defined(ENABLE_SAFE_BROWSING)
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
#else // ENABLE_SAFE_BROWSING
  // If safe browsing is not enabled, then these tests would fail. If such a
  // test was added, then fail early.
  EXPECT_NE(content::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL, test_case.danger_type);
  EXPECT_NE(content::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT,
            test_case.danger_type);
#endif // !ENABLE_SAFE_BROWSING

  DownloadItem::TargetDisposition initial_disposition =
      (test_case.test_type == SAVE_AS) ?
          DownloadItem::TARGET_DISPOSITION_PROMPT :
          DownloadItem::TARGET_DISPOSITION_OVERWRITE;
  EXPECT_CALL(*item, GetTargetFilePath())
      .WillRepeatedly(ReturnRefOfCopy(FilePath()));
  EXPECT_CALL(*item, GetTargetDisposition())
      .WillRepeatedly(Return(initial_disposition));
  EXPECT_CALL(*item, GetDangerType())
      .WillRepeatedly(Return(content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS));

  if (test_case.expected_disposition ==
      DownloadItem::TARGET_DISPOSITION_PROMPT) {
    FilePath expected_prompt_path = GetPathInDownloadDir(
        test_case.expected_prompt_path);
    FilePath expected_target_path = GetPathInDownloadDir(
        test_case.expected_target_path);
    EXPECT_CALL(*delegate_,
                ChooseDownloadPath(item, expected_prompt_path, _))
        .WillOnce(WithArg<2>(ScheduleCallback(expected_target_path)));
  }

  // Kick off the test.
  base::WeakPtrFactory<ChromeDownloadManagerDelegateTest> factory(this);
  EXPECT_TRUE(delegate_->DetermineDownloadTarget(
      item,
      base::Bind(&ChromeDownloadManagerDelegateTest::DownloadTargetVerifier,
                 factory.GetWeakPtr(), base::Unretained(&test_case))));
  message_loop_.RunAllPending();
  VerifyAndClearExpectations();
}

void ChromeDownloadManagerDelegateTest::RunTestCases(
    const DownloadTestCase test_cases[],
    size_t test_case_count) {
  for (size_t i = 0; i < test_case_count; ++i) {
    scoped_ptr<content::MockDownloadItem> item(CreateActiveDownloadItem(i));
    SCOPED_TRACE(testing::Message() << "Running test case " << i);
    RunTestCaseWithDownloadItem(test_cases[i], item.get());
  }
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

void ChromeDownloadManagerDelegateTest::DownloadTargetVerifier(
    const DownloadTestCase* test_case,
    const FilePath& target_path,
    DownloadItem::TargetDisposition disposition,
    content::DownloadDangerType danger_type,
    const FilePath& intermediate_path) {
  FilePath expected_target_path(
      GetPathInDownloadDir(test_case->expected_target_path));
  EXPECT_EQ(expected_target_path.value(), target_path.value());
  EXPECT_EQ(test_case->expected_disposition, disposition);
  EXPECT_EQ(test_case->danger_type, danger_type);

  if (test_case->expected_intermediate == EXPECT_CRDOWNLOAD) {
    EXPECT_EQ(download_util::GetCrDownloadPath(target_path).value(),
              intermediate_path.value());
  } else {
    // The paths (in English) look like: /path/Unconfirmed xxx.crdownload.
    // Of this, we only check that the path is:
    // 1. Not "/path/target.crdownload",
    // 2. Points to the same directory as the target.
    // 3. Has extension ".crdownload".
    EXPECT_NE(download_util::GetCrDownloadPath(target_path).value(),
              intermediate_path.value());
    EXPECT_EQ(target_path.DirName().value(),
              intermediate_path.DirName().value());
    EXPECT_TRUE(intermediate_path.MatchesExtension(
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

}  // namespace

TEST_F(ChromeDownloadManagerDelegateTest, StartDownload_Basic) {
  const DownloadTestCase kBasicTestCases[] = {
    {
      // 0: Automatic Safe
      AUTOMATIC,
      content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
      "http://example.com/foo.txt", "text/plain",
      FILE_PATH_LITERAL(""),

      FILE_PATH_LITERAL("foo.txt"),
      FILE_PATH_LITERAL(""),
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
      FILE_PATH_LITERAL("foo.txt"),
      DownloadItem::TARGET_DISPOSITION_PROMPT,

      EXPECT_CRDOWNLOAD
    },

    {
      // 2: Automatic Dangerous
      AUTOMATIC,
      content::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE,
      "http://example.com/foo.exe", "",
      FILE_PATH_LITERAL(""),

      FILE_PATH_LITERAL("foo.exe"),
      FILE_PATH_LITERAL(""),
      DownloadItem::TARGET_DISPOSITION_OVERWRITE,

      EXPECT_UNCONFIRMED
    },

    {
      // 3 Forced Safe
      FORCED,
      content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
      "http://example.com/foo.txt", "",
      FILE_PATH_LITERAL("forced-foo.txt"),

      FILE_PATH_LITERAL("forced-foo.txt"),
      FILE_PATH_LITERAL(""),
      DownloadItem::TARGET_DISPOSITION_OVERWRITE,

      EXPECT_CRDOWNLOAD
    },

#if defined(ENABLE_SAFE_BROWSING)
    // These test cases are only applicable if safe browsing is enabled. Without
    // it, these are equivalent to FORCED/SAFE and SAFE_AS/SAFE respectively.
    {
      // 4: Forced Dangerous. As above. .jar is considered to be one of the file
      // types supportred by safe browsing.
      FORCED,
      content::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT,
      "http://example.com/foo.exe", "",
      FILE_PATH_LITERAL("forced-foo.exe"),

      FILE_PATH_LITERAL("forced-foo.exe"),
      FILE_PATH_LITERAL(""),
      DownloadItem::TARGET_DISPOSITION_OVERWRITE,

      EXPECT_UNCONFIRMED
    },

    {
      // 5: Save_As Dangerous.
      SAVE_AS,
      content::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT,
      "http://example.com/foo.exe", "",
      FILE_PATH_LITERAL(""),

      FILE_PATH_LITERAL("foo.exe"),
      FILE_PATH_LITERAL("foo.exe"),
      DownloadItem::TARGET_DISPOSITION_PROMPT,

      EXPECT_UNCONFIRMED
    }
#endif
  };

  RunTestCases(kBasicTestCases, arraysize(kBasicTestCases));
}

#if defined(ENABLE_SAFE_BROWSING)
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
      FILE_PATH_LITERAL(""),
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
      FILE_PATH_LITERAL(""),
      DownloadItem::TARGET_DISPOSITION_OVERWRITE,

      EXPECT_UNCONFIRMED
    },

    {
      // 3: Automatic Dangerous URL + Dangerous file. Dangerous URL takes
      // precendence.
      AUTOMATIC,
      content::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL,
      "http://phishing.example.com/foo.jar", "",
      FILE_PATH_LITERAL(""),

      FILE_PATH_LITERAL("foo.jar"),
      FILE_PATH_LITERAL(""),
      DownloadItem::TARGET_DISPOSITION_OVERWRITE,

      EXPECT_UNCONFIRMED
    },

    {
      // 4: Save As Dangerous URL + Dangerous file
      SAVE_AS,
      content::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL,
      "http://phishing.example.com/foo.jar", "",
      FILE_PATH_LITERAL(""),

      FILE_PATH_LITERAL("foo.jar"),
      FILE_PATH_LITERAL("foo.jar"),
      DownloadItem::TARGET_DISPOSITION_PROMPT,

      EXPECT_UNCONFIRMED
    },

    {
      // 5: Forced Dangerous URL + Dangerous file
      FORCED,
      content::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL,
      "http://phishing.example.com/foo.jar", "",
      FILE_PATH_LITERAL("forced-foo.jar"),

      FILE_PATH_LITERAL("forced-foo.jar"),
      FILE_PATH_LITERAL(""),
      DownloadItem::TARGET_DISPOSITION_OVERWRITE,

      EXPECT_UNCONFIRMED
    },
  };

  RunTestCases(kDangerousURLTestCases, arraysize(kDangerousURLTestCases));
}
#endif  // ENABLE_SAFE_BROWSING

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
      FILE_PATH_LITERAL("foo.txt"),
      DownloadItem::TARGET_DISPOSITION_PROMPT,

      EXPECT_CRDOWNLOAD
    },

    {
      // 1: Automatic Browser Extension download. - Shouldn't prompt for browser
      //    extension downloads even if "Prompt for download" preference is set.
      AUTOMATIC,
      content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
      "http://example.com/foo.crx",
      extensions::Extension::kMimeType,
      FILE_PATH_LITERAL(""),

      FILE_PATH_LITERAL("foo.crx"),
      FILE_PATH_LITERAL(""),
      DownloadItem::TARGET_DISPOSITION_OVERWRITE,

      EXPECT_CRDOWNLOAD
    },

    {
      // 2: Automatic User Script - Shouldn't prompt for user script downloads
      //    even if "Prompt for download" preference is set.
      AUTOMATIC,
      content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
      "http://example.com/foo.user.js", "",
      FILE_PATH_LITERAL(""),

      FILE_PATH_LITERAL("foo.user.js"),
      FILE_PATH_LITERAL(""),
      DownloadItem::TARGET_DISPOSITION_OVERWRITE,

      EXPECT_CRDOWNLOAD
    },

    {
      // 3: Automatic - The filename extension is marked as one that we will
      //    open automatically. Shouldn't prompt.
      AUTOMATIC,
      content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
      "http://example.com/foo.dummy", "",
      FILE_PATH_LITERAL(""),

      FILE_PATH_LITERAL("foo.dummy"),
      FILE_PATH_LITERAL(""),
      DownloadItem::TARGET_DISPOSITION_OVERWRITE,

      EXPECT_CRDOWNLOAD
    },

#if defined(ENABLE_SAFE_BROWSING)
    // If safe browsing is disabled, this case is equivalent to AUTOMATIC/SAFE
    // since the download isn't marked as dangerous when we are going to prompt
    // the user.
    {
      // 4: Dangerous Automatic - Should prompt due to "Prompt for download"
      //    preference setting.
      AUTOMATIC,
      content::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT,
      "http://example.com/foo.exe", "",
      FILE_PATH_LITERAL(""),

      FILE_PATH_LITERAL("foo.exe"),
      FILE_PATH_LITERAL("foo.exe"),
      DownloadItem::TARGET_DISPOSITION_PROMPT,

      EXPECT_UNCONFIRMED
    },
#endif
  };

  SetPromptForDownload(true);
  EnableAutoOpenBasedOnExtension(FilePath(FILE_PATH_LITERAL("dummy.dummy")));
  RunTestCases(kPromptingTestCases, arraysize(kPromptingTestCases));
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
      FILE_PATH_LITERAL(""),
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
      FILE_PATH_LITERAL(""),
      DownloadItem::TARGET_DISPOSITION_OVERWRITE,

      EXPECT_CRDOWNLOAD
    },
  };

  SetManagedDownloadPath(default_download_path());
  ASSERT_TRUE(download_prefs()->IsDownloadPathManaged());
  RunTestCases(kManagedPathTestCases, arraysize(kManagedPathTestCases));
}

// Test whether the last saved directory is saved if the user was presented with
// a file chooser.
TEST_F(ChromeDownloadManagerDelegateTest, StartDownload_LastSavePath) {
  const DownloadTestCase kLastSavePathTestCases[] = {
    {
      // Initially the last saved directory is the user's default download path.

      // 0: Start a Save As download. Then respond to the file chooser with
      //    foo/bar.txt as the target directory. This should cause the foo/
      //    directory to be remembered as the last used save directory.
      SAVE_AS,
      content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
      "http://example.com/foo.txt", "text/plain",
      FILE_PATH_LITERAL(""),

      FILE_PATH_LITERAL("foo/bar.txt"),
      FILE_PATH_LITERAL("foo.txt"),
      DownloadItem::TARGET_DISPOSITION_PROMPT,

      EXPECT_CRDOWNLOAD
    },

    {
      // 1: Start another Save As download. This time the suggested path should
      //    be in the foo/ directory.
      SAVE_AS,
      content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
      "http://example.com/foo.txt", "text/plain",
      FILE_PATH_LITERAL(""),

      FILE_PATH_LITERAL("bar/foo.txt"),
      FILE_PATH_LITERAL("foo/foo.txt"),
      DownloadItem::TARGET_DISPOSITION_PROMPT,

      EXPECT_CRDOWNLOAD
    },

    {
      // 2: Start an automatic download. This should be saved to the user's
      //    default download directory and not the last used Save As directory.
      AUTOMATIC,
      content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
      "http://example.com/foo.txt", "text/plain",
      FILE_PATH_LITERAL(""),

      FILE_PATH_LITERAL("foo.txt"),
      FILE_PATH_LITERAL(""),
      DownloadItem::TARGET_DISPOSITION_OVERWRITE,

      EXPECT_CRDOWNLOAD
    },
  };

  RunTestCases(kLastSavePathTestCases, arraysize(kLastSavePathTestCases));

  // Now clear the last download path.
  delegate()->ClearLastDownloadPath();

  // Run the first test case again. Since the last download path was cleared,
  // this test case should behave identically to the first time it was run.
  RunTestCases(kLastSavePathTestCases, 1);
}

TEST_F(ChromeDownloadManagerDelegateTest, StartDownload_WebIntents) {
  const DownloadTestCase kWebIntentsTestCases[] = {
    {
      // 1: A file which would be dangerous, but is handled by web intents.
      // The name will be unaltered (the actual save name will have the
      // .webintents extension).
      AUTOMATIC,
      content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
      "http://example.com/feed.exe", "application/rss+xml",
      FILE_PATH_LITERAL(""),

      FILE_PATH_LITERAL("feed.exe.webintents"),
      FILE_PATH_LITERAL(""),
      DownloadItem::TARGET_DISPOSITION_OVERWRITE,

      EXPECT_CRDOWNLOAD
    },

    {
      // 2: A download with a forced path won't be handled by web intents.
      FORCED,
      content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
      "http://example.com/feed.exe", "application/rss+xml",
      FILE_PATH_LITERAL("forced.feed.exe"),

      FILE_PATH_LITERAL("forced.feed.exe"),
      FILE_PATH_LITERAL(""),
      DownloadItem::TARGET_DISPOSITION_OVERWRITE,

      EXPECT_CRDOWNLOAD
    },
  };

  RunTestCases(kWebIntentsTestCases, arraysize(kWebIntentsTestCases));
}

// TODO(asanka): Add more tests.
// * Default download path is not writable.
// * Download path doesn't exist.
// * IsDangerousFile().
// * Filename generation.
