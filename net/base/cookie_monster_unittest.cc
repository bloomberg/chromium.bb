// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <time.h>

#include <algorithm>
#include <string>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
#include "base/time.h"
#include "googleurl/src/gurl.h"
#include "net/base/cookie_monster.h"
#include "net/base/cookie_monster_store_test.h"  // For CookieStore mock
#include "net/base/cookie_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

using base::Time;
using base::TimeDelta;
using base::Thread;

namespace {

// TODO(erikwright): Replace the pre-existing MockPersistentCookieStore (and
// brethren) with this one, and remove the 'New' prefix.
class NewMockPersistentCookieStore
    : public CookieMonster::PersistentCookieStore {
 public:
  MOCK_METHOD1(Load, void(const LoadedCallback& loaded_callback));
  MOCK_METHOD2(LoadCookiesForKey, void(const std::string& key,
                                       const LoadedCallback& loaded_callback));
  MOCK_METHOD1(AddCookie, void(const CookieMonster::CanonicalCookie& cc));
  MOCK_METHOD1(UpdateCookieAccessTime,
               void(const CookieMonster::CanonicalCookie& cc));
  MOCK_METHOD1(DeleteCookie, void(const CookieMonster::CanonicalCookie& cc));
  MOCK_METHOD1(SetClearLocalStateOnExit, void(bool clear_local_state));
  MOCK_METHOD1(Flush, void(Task* completion_task));
};

const int kTimeout = 1000;

const char* kTopLevelDomainPlus1 = "http://www.harvard.edu";
const char* kTopLevelDomainPlus2 = "http://www.math.harvard.edu";
const char* kTopLevelDomainPlus2Secure = "https://www.math.harvard.edu";
const char* kTopLevelDomainPlus3 =
    "http://www.bourbaki.math.harvard.edu";
const char* kOtherDomain = "http://www.mit.edu";

const char kUrlGoogle[] = "http://www.google.izzle";
const char kUrlGoogleFoo[] = "http://www.google.izzle/foo";
const char kUrlGoogleBar[] = "http://www.google.izzle/bar";
const char kUrlGoogleSpecific[] = "http://www.gmail.google.izzle";
const char kUrlGoogleSecure[] = "https://www.google.izzle";
const char kUrlFtp[] = "ftp://ftp.google.izzle/";
const char kValidCookieLine[] = "A=B; path=/";
const char kValidDomainCookieLine[] = "A=B; path=/; domain=google.izzle";

// Defines common behaviour for the callbacks from GetCookies, SetCookies, etc.
// Asserts that the current thread is the expected invocation thread, sends a
// quit to the thread in which it was constructed.
class CookieCallback {
 public:
  // Indicates whether the callback has been called.
  bool did_run() { return did_run_; }

 protected:
  // Constructs a callback that expects to be called in the given thread and
  // will, upon execution, send a QUIT to the constructing thread.
  explicit CookieCallback(Thread* run_in_thread)
      : did_run_(false),
        run_in_thread_(run_in_thread),
        run_in_loop_(NULL),
        parent_loop_(MessageLoop::current()),
        loop_to_quit_(MessageLoop::current()) {}

  // Constructs a callback that expects to be called in current thread and will
  // send a QUIT to the constructing thread.
  CookieCallback()
      : did_run_(false),
        run_in_thread_(NULL),
        run_in_loop_(MessageLoop::current()),
        parent_loop_(NULL),
        loop_to_quit_(MessageLoop::current()) {}

  // Tests whether the current thread was the caller's thread.
  // Sends a QUIT to the constructing thread.
  void CallbackEpilogue() {
    MessageLoop* expected_loop = NULL;
    if (run_in_thread_) {
      DCHECK(!run_in_loop_);
      expected_loop = run_in_thread_->message_loop();
    } else if (run_in_loop_) {
      expected_loop = run_in_loop_;
    }
    ASSERT_TRUE(expected_loop != NULL);

    did_run_ = true;
    EXPECT_EQ(expected_loop, MessageLoop::current());
    loop_to_quit_->PostTask(FROM_HERE, new MessageLoop::QuitTask);
  }

 private:
  bool did_run_;
  Thread* run_in_thread_;
  MessageLoop* run_in_loop_;
  MessageLoop* parent_loop_;
  MessageLoop* loop_to_quit_;
};

// Callback implementations for the asynchronous CookieMonster methods.

class SetCookieCallback : public CookieCallback {
 public:
  SetCookieCallback() : result_(false) {}
  explicit SetCookieCallback(Thread* run_in_thread)
      : CookieCallback(run_in_thread),
        result_(false) {}

  void Run(bool result) {
    result_ = result;
    CallbackEpilogue();
  }

  bool result() { return result_; }

 private:
  bool result_;
};

class GetCookieStringCallback : public CookieCallback {
 public:
  GetCookieStringCallback() {}
  explicit GetCookieStringCallback(Thread* run_in_thread)
      : CookieCallback(run_in_thread) {}

  void Run(std::string cookie) {
    cookie_ = cookie;
    CallbackEpilogue();
  }

  const std::string& cookie() { return cookie_; }

 private:
  std::string cookie_;
};

class GetCookiesWithInfoCallback : public CookieCallback {
 public:
  GetCookiesWithInfoCallback() {}
  explicit GetCookiesWithInfoCallback(Thread* run_in_thread)
      : CookieCallback(run_in_thread) {}

  void Run(
      const std::string& cookie_line,
      const std::vector<CookieStore::CookieInfo>& cookie_info) {
    cookie_line_ = cookie_line;
    cookie_info_ = cookie_info;
    CallbackEpilogue();
  }

  const std::string& cookie_line() { return cookie_line_; }
  const std::vector<CookieStore::CookieInfo>& cookie_info() {
    return cookie_info_;
  }

 private:
  std::string cookie_line_;
  std::vector<CookieStore::CookieInfo> cookie_info_;
};

class  GetCookieListCallback : public CookieCallback {
 public:
  GetCookieListCallback() {}
  explicit GetCookieListCallback(Thread* run_in_thread)
      : CookieCallback(run_in_thread) {}

  void Run(const CookieList& cookies) {
    cookies_ = cookies;
    CallbackEpilogue();
  }

  const CookieList& cookies() { return cookies_; }

 private:
  CookieList cookies_;
};

class  DeleteCallback : public CookieCallback {
 public:
  DeleteCallback() : num_deleted_(0) {}
  explicit DeleteCallback(Thread* run_in_thread)
      : CookieCallback(run_in_thread),
        num_deleted_(0) {}

  void Run(int num_deleted) {
    num_deleted_ = num_deleted;
    CallbackEpilogue();
  }

  int num_deleted() { return num_deleted_; }

 private:
  int num_deleted_;
};

class  DeleteCookieCallback : public CookieCallback {
 public:
  DeleteCookieCallback() {}
  explicit DeleteCookieCallback(Thread* run_in_thread)
      : CookieCallback(run_in_thread) {}

  void Run() {
    CallbackEpilogue();
  }
};

class ParsedCookieTest : public testing::Test { };

}  // namespace

TEST(ParsedCookieTest, TestBasic) {
  CookieMonster::ParsedCookie pc("a=b");
  EXPECT_TRUE(pc.IsValid());
  EXPECT_FALSE(pc.IsSecure());
  EXPECT_EQ("a", pc.Name());
  EXPECT_EQ("b", pc.Value());
}

TEST(ParsedCookieTest, TestQuoted) {
  // These are some quoting cases which the major browsers all
  // handle differently.  I've tested Internet Explorer 6, Opera 9.6,
  // Firefox 3, and Safari Windows 3.2.1.  We originally tried to match
  // Firefox closely, however we now match Internet Explorer and Safari.
  const char* values[] = {
    // Trailing whitespace after a quoted value.  The whitespace after
    // the quote is stripped in all browsers.
    "\"zzz \"  ",              "\"zzz \"",
    // Handling a quoted value with a ';', like FOO="zz;pp"  ;
    // IE and Safari: "zz;
    // Firefox and Opera: "zz;pp"
    "\"zz;pp\" ;",             "\"zz",
    // Handling a value with multiple quoted parts, like FOO="zzz "   "ppp" ;
    // IE and Safari: "zzz "   "ppp";
    // Firefox: "zzz ";
    // Opera: <rejects cookie>
    "\"zzz \"   \"ppp\" ",     "\"zzz \"   \"ppp\"",
    // A quote in a value that didn't start quoted.  like FOO=A"B ;
    // IE, Safari, and Firefox: A"B;
    // Opera: <rejects cookie>
    "A\"B",                    "A\"B",
  };

  for (size_t i = 0; i < arraysize(values); i += 2) {
    std::string input(values[i]);
    std::string expected(values[i + 1]);

    CookieMonster::ParsedCookie pc(
        "aBc=" + input + " ; path=\"/\"  ; httponly ");
    EXPECT_TRUE(pc.IsValid());
    EXPECT_FALSE(pc.IsSecure());
    EXPECT_TRUE(pc.IsHttpOnly());
    EXPECT_TRUE(pc.HasPath());
    EXPECT_EQ("aBc", pc.Name());
    EXPECT_EQ(expected, pc.Value());

    // If a path was quoted, the path attribute keeps the quotes.  This will
    // make the cookie effectively useless, but path parameters aren't supposed
    // to be quoted.  Bug 1261605.
    EXPECT_EQ("\"/\"", pc.Path());
  }
}

TEST(ParsedCookieTest, TestNameless) {
  CookieMonster::ParsedCookie pc("BLAHHH; path=/; secure;");
  EXPECT_TRUE(pc.IsValid());
  EXPECT_TRUE(pc.IsSecure());
  EXPECT_TRUE(pc.HasPath());
  EXPECT_EQ("/", pc.Path());
  EXPECT_EQ("", pc.Name());
  EXPECT_EQ("BLAHHH", pc.Value());
}

TEST(ParsedCookieTest, TestAttributeCase) {
  CookieMonster::ParsedCookie pc("BLAHHH; Path=/; sECuRe; httpONLY");
  EXPECT_TRUE(pc.IsValid());
  EXPECT_TRUE(pc.IsSecure());
  EXPECT_TRUE(pc.IsHttpOnly());
  EXPECT_TRUE(pc.HasPath());
  EXPECT_EQ("/", pc.Path());
  EXPECT_EQ("", pc.Name());
  EXPECT_EQ("BLAHHH", pc.Value());
  EXPECT_EQ(3U, pc.NumberOfAttributes());
}

TEST(ParsedCookieTest, TestDoubleQuotedNameless) {
  CookieMonster::ParsedCookie pc("\"BLA\\\"HHH\"; path=/; secure;");
  EXPECT_TRUE(pc.IsValid());
  EXPECT_TRUE(pc.IsSecure());
  EXPECT_TRUE(pc.HasPath());
  EXPECT_EQ("/", pc.Path());
  EXPECT_EQ("", pc.Name());
  EXPECT_EQ("\"BLA\\\"HHH\"", pc.Value());
  EXPECT_EQ(2U, pc.NumberOfAttributes());
}

TEST(ParsedCookieTest, QuoteOffTheEnd) {
  CookieMonster::ParsedCookie pc("a=\"B");
  EXPECT_TRUE(pc.IsValid());
  EXPECT_EQ("a", pc.Name());
  EXPECT_EQ("\"B", pc.Value());
  EXPECT_EQ(0U, pc.NumberOfAttributes());
}

TEST(ParsedCookieTest, MissingName) {
  CookieMonster::ParsedCookie pc("=ABC");
  EXPECT_TRUE(pc.IsValid());
  EXPECT_EQ("", pc.Name());
  EXPECT_EQ("ABC", pc.Value());
  EXPECT_EQ(0U, pc.NumberOfAttributes());
}

TEST(ParsedCookieTest, MissingValue) {
  CookieMonster::ParsedCookie pc("ABC=;  path = /wee");
  EXPECT_TRUE(pc.IsValid());
  EXPECT_EQ("ABC", pc.Name());
  EXPECT_EQ("", pc.Value());
  EXPECT_TRUE(pc.HasPath());
  EXPECT_EQ("/wee", pc.Path());
  EXPECT_EQ(1U, pc.NumberOfAttributes());
}

TEST(ParsedCookieTest, Whitespace) {
  CookieMonster::ParsedCookie pc("  A  = BC  ;secure;;;   httponly");
  EXPECT_TRUE(pc.IsValid());
  EXPECT_EQ("A", pc.Name());
  EXPECT_EQ("BC", pc.Value());
  EXPECT_FALSE(pc.HasPath());
  EXPECT_FALSE(pc.HasDomain());
  EXPECT_TRUE(pc.IsSecure());
  EXPECT_TRUE(pc.IsHttpOnly());
  // We parse anything between ; as attributes, so we end up with two
  // attributes with an empty string name and value.
  EXPECT_EQ(4U, pc.NumberOfAttributes());
}
TEST(ParsedCookieTest, MultipleEquals) {
  CookieMonster::ParsedCookie pc("  A=== BC  ;secure;;;   httponly");
  EXPECT_TRUE(pc.IsValid());
  EXPECT_EQ("A", pc.Name());
  EXPECT_EQ("== BC", pc.Value());
  EXPECT_FALSE(pc.HasPath());
  EXPECT_FALSE(pc.HasDomain());
  EXPECT_TRUE(pc.IsSecure());
  EXPECT_TRUE(pc.IsHttpOnly());
  EXPECT_EQ(4U, pc.NumberOfAttributes());
}

TEST(ParsedCookieTest, MACKey) {
  CookieMonster::ParsedCookie pc("foo=bar; MAC-Key=3900ac9anw9incvw9f");
  EXPECT_TRUE(pc.IsValid());
  EXPECT_EQ("foo", pc.Name());
  EXPECT_EQ("bar", pc.Value());
  EXPECT_EQ("3900ac9anw9incvw9f", pc.MACKey());
  EXPECT_EQ(1U, pc.NumberOfAttributes());
}

TEST(ParsedCookieTest, MACAlgorithm) {
  CookieMonster::ParsedCookie pc("foo=bar; MAC-Algorithm=hmac-sha-1");
  EXPECT_TRUE(pc.IsValid());
  EXPECT_EQ("foo", pc.Name());
  EXPECT_EQ("bar", pc.Value());
  EXPECT_EQ("hmac-sha-1", pc.MACAlgorithm());
  EXPECT_EQ(1U, pc.NumberOfAttributes());
}

TEST(ParsedCookieTest, MACKeyAndMACAlgorithm) {
  CookieMonster::ParsedCookie pc(
        "foo=bar; MAC-Key=voiae-09fj0302nfqf; MAC-Algorithm=hmac-sha-256");
  EXPECT_TRUE(pc.IsValid());
  EXPECT_EQ("foo", pc.Name());
  EXPECT_EQ("bar", pc.Value());
  EXPECT_EQ("voiae-09fj0302nfqf", pc.MACKey());
  EXPECT_EQ("hmac-sha-256", pc.MACAlgorithm());
  EXPECT_EQ(2U, pc.NumberOfAttributes());
}

TEST(ParsedCookieTest, QuotedTrailingWhitespace) {
  CookieMonster::ParsedCookie pc("ANCUUID=\"zohNumRKgI0oxyhSsV3Z7D\"  ; "
                                      "expires=Sun, 18-Apr-2027 21:06:29 GMT ; "
                                      "path=/  ;  ");
  EXPECT_TRUE(pc.IsValid());
  EXPECT_EQ("ANCUUID", pc.Name());
  // Stripping whitespace after the quotes matches all other major browsers.
  EXPECT_EQ("\"zohNumRKgI0oxyhSsV3Z7D\"", pc.Value());
  EXPECT_TRUE(pc.HasExpires());
  EXPECT_TRUE(pc.HasPath());
  EXPECT_EQ("/", pc.Path());
  EXPECT_EQ(2U, pc.NumberOfAttributes());
}

TEST(ParsedCookieTest, TrailingWhitespace) {
  CookieMonster::ParsedCookie pc("ANCUUID=zohNumRKgI0oxyhSsV3Z7D  ; "
                                      "expires=Sun, 18-Apr-2027 21:06:29 GMT ; "
                                      "path=/  ;  ");
  EXPECT_TRUE(pc.IsValid());
  EXPECT_EQ("ANCUUID", pc.Name());
  EXPECT_EQ("zohNumRKgI0oxyhSsV3Z7D", pc.Value());
  EXPECT_TRUE(pc.HasExpires());
  EXPECT_TRUE(pc.HasPath());
  EXPECT_EQ("/", pc.Path());
  EXPECT_EQ(2U, pc.NumberOfAttributes());
}

TEST(ParsedCookieTest, TooManyPairs) {
  std::string blankpairs;
  blankpairs.resize(CookieMonster::ParsedCookie::kMaxPairs - 1, ';');

  CookieMonster::ParsedCookie pc1(blankpairs + "secure");
  EXPECT_TRUE(pc1.IsValid());
  EXPECT_TRUE(pc1.IsSecure());

  CookieMonster::ParsedCookie pc2(blankpairs + ";secure");
  EXPECT_TRUE(pc2.IsValid());
  EXPECT_FALSE(pc2.IsSecure());
}

// TODO(erikwright): some better test cases for invalid cookies.
TEST(ParsedCookieTest, InvalidWhitespace) {
  CookieMonster::ParsedCookie pc("    ");
  EXPECT_FALSE(pc.IsValid());
}

TEST(ParsedCookieTest, InvalidTooLong) {
  std::string maxstr;
  maxstr.resize(CookieMonster::ParsedCookie::kMaxCookieSize, 'a');

  CookieMonster::ParsedCookie pc1(maxstr);
  EXPECT_TRUE(pc1.IsValid());

  CookieMonster::ParsedCookie pc2(maxstr + "A");
  EXPECT_FALSE(pc2.IsValid());
}

TEST(ParsedCookieTest, InvalidEmpty) {
  CookieMonster::ParsedCookie pc("");
  EXPECT_FALSE(pc.IsValid());
}

TEST(ParsedCookieTest, EmbeddedTerminator) {
  CookieMonster::ParsedCookie pc1("AAA=BB\0ZYX");
  CookieMonster::ParsedCookie pc2("AAA=BB\rZYX");
  CookieMonster::ParsedCookie pc3("AAA=BB\nZYX");
  EXPECT_TRUE(pc1.IsValid());
  EXPECT_EQ("AAA", pc1.Name());
  EXPECT_EQ("BB", pc1.Value());
  EXPECT_TRUE(pc2.IsValid());
  EXPECT_EQ("AAA", pc2.Name());
  EXPECT_EQ("BB", pc2.Value());
  EXPECT_TRUE(pc3.IsValid());
  EXPECT_EQ("AAA", pc3.Name());
  EXPECT_EQ("BB", pc3.Value());
}

TEST(ParsedCookieTest, ParseTokensAndValues) {
  EXPECT_EQ("hello",
            CookieMonster::ParsedCookie::ParseTokenString(
                "hello\nworld"));
  EXPECT_EQ("fs!!@",
            CookieMonster::ParsedCookie::ParseTokenString(
                "fs!!@;helloworld"));
  EXPECT_EQ("hello world\tgood",
            CookieMonster::ParsedCookie::ParseTokenString(
                "hello world\tgood\rbye"));
  EXPECT_EQ("A",
            CookieMonster::ParsedCookie::ParseTokenString(
                "A=B=C;D=E"));
  EXPECT_EQ("hello",
            CookieMonster::ParsedCookie::ParseValueString(
                "hello\nworld"));
  EXPECT_EQ("fs!!@",
            CookieMonster::ParsedCookie::ParseValueString(
                "fs!!@;helloworld"));
  EXPECT_EQ("hello world\tgood",
            CookieMonster::ParsedCookie::ParseValueString(
                "hello world\tgood\rbye"));
  EXPECT_EQ("A=B=C",
            CookieMonster::ParsedCookie::ParseValueString(
                "A=B=C;D=E"));
}

namespace {

class CookieMonsterTest : public testing::Test {
 protected:
  CookieMonsterTest()
      : url_google_(kUrlGoogle),
        url_google_secure_(kUrlGoogleSecure),
        url_google_foo_(kUrlGoogleFoo),
        url_google_bar_(kUrlGoogleBar),
        message_loop_factory_(MessageLoop::current()) {}

  // Helper methods for the asynchronous Cookie Store API that call the
  // asynchronous method and then pump the loop until the callback is invoked,
  // finally returning the value.

  std::string GetCookies(CookieMonster* cm, const GURL& url) {
    DCHECK(cm);
    GetCookieStringCallback callback;
    cm->GetCookiesWithOptionsAsync(
        url, CookieOptions(), base::Bind(&GetCookieStringCallback::Run,
                                         base::Unretained(&callback)));
    RunFor(kTimeout);
    EXPECT_TRUE(callback.did_run());
    return callback.cookie();
  }

  std::string GetCookiesWithOptions(CookieMonster* cm,
                                    const GURL& url,
                                    const CookieOptions& options) {
    DCHECK(cm);
    GetCookieStringCallback callback;
    cm->GetCookiesWithOptionsAsync(
        url, options,
        base::Bind(&GetCookieStringCallback::Run,
                   base::Unretained(&callback)));
    RunFor(kTimeout);
    EXPECT_TRUE(callback.did_run());
    return callback.cookie();
  }

  void GetCookiesWithInfo(CookieMonster* cm,
                          const GURL& url,
                          const CookieOptions& options,
                          std::string* cookie_line,
                          std::vector<CookieStore::CookieInfo>* cookie_info) {
    DCHECK(cm);
    DCHECK(cookie_line);
    DCHECK(cookie_info);
    GetCookiesWithInfoCallback callback;
    cm->GetCookiesWithInfoAsync(
        url, options, base::Bind(&GetCookiesWithInfoCallback::Run,
                                 base::Unretained(&callback)));
    RunFor(kTimeout);
    EXPECT_TRUE(callback.did_run());
    *cookie_line = callback.cookie_line();
    *cookie_info = callback.cookie_info();
  }

  CookieList GetAllCookies(CookieMonster* cm) {
    DCHECK(cm);
    GetCookieListCallback callback;
    cm->GetAllCookiesAsync(
        base::Bind(&GetCookieListCallback::Run,
                   base::Unretained(&callback)));
    RunFor(kTimeout);
    EXPECT_TRUE(callback.did_run());
    return callback.cookies();
  }

  CookieList GetAllCookiesForURL(CookieMonster* cm,
                                 const GURL& url) {
    DCHECK(cm);
    GetCookieListCallback callback;
    cm->GetAllCookiesForURLAsync(
        url, base::Bind(&GetCookieListCallback::Run,
                        base::Unretained(&callback)));
    RunFor(kTimeout);
    EXPECT_TRUE(callback.did_run());
    return callback.cookies();
  }

  CookieList GetAllCookiesForURLWithOptions(CookieMonster* cm,
                                            const GURL& url,
                                            const CookieOptions& options) {
    DCHECK(cm);
    GetCookieListCallback callback;
    cm->GetAllCookiesForURLWithOptionsAsync(
        url, options, base::Bind(&GetCookieListCallback::Run,
                                 base::Unretained(&callback)));
    RunFor(kTimeout);
    EXPECT_TRUE(callback.did_run());
    return callback.cookies();
  }

  bool SetCookieWithOptions(CookieMonster* cm,
                            const GURL& url,
                            const std::string& cookie_line,
                            const CookieOptions& options) {
    DCHECK(cm);
    SetCookieCallback callback;
    cm->SetCookieWithOptionsAsync(
        url, cookie_line, options,
        base::Bind(&SetCookieCallback::Run, base::Unretained(&callback)));
    RunFor(kTimeout);
    EXPECT_TRUE(callback.did_run());
    return callback.result();
  }

  bool SetCookie(CookieMonster* cm,
                 const GURL& url,
                 const std::string& cookie_line) {
    CookieOptions options;
    return SetCookieWithOptions(cm, url, cookie_line, options);
  }

  bool SetCookieWithDetails(CookieMonster* cm,
                            const GURL& url,
                            const std::string& name,
                            const std::string& value,
                            const std::string& domain,
                            const std::string& path,
                            const base::Time& expiration_time,
                            bool secure, bool http_only) {
    DCHECK(cm);
    SetCookieCallback callback;
    cm->SetCookieWithDetailsAsync(
        url, name, value, domain, path, expiration_time, secure, http_only,
        base::Bind(&SetCookieCallback::Run, base::Unretained(&callback)));
    RunFor(kTimeout);
    EXPECT_TRUE(callback.did_run());
    return callback.result();
  }

  void DeleteCookie(CookieMonster* cm,
                    const GURL& url,
                    const std::string& cookie_name) {
    DCHECK(cm);
    DeleteCookieCallback callback;
    cm->DeleteCookieAsync(url, cookie_name,
                          base::Bind(&DeleteCookieCallback::Run,
                                     base::Unretained(&callback)));
    RunFor(kTimeout);
    EXPECT_TRUE(callback.did_run());
  }

  int DeleteAll(CookieMonster*cm) {
    DCHECK(cm);
    DeleteCallback callback;
    cm->DeleteAllAsync(
        base::Bind(&DeleteCallback::Run, base::Unretained(&callback)));
    RunFor(kTimeout);
    EXPECT_TRUE(callback.did_run());
    return callback.num_deleted();
  }

  int DeleteAllCreatedBetween(CookieMonster*cm,
                              const base::Time& delete_begin,
                              const base::Time& delete_end) {
    DCHECK(cm);
    DeleteCallback callback;
    cm->DeleteAllCreatedBetweenAsync(
        delete_begin, delete_end,
        base::Bind(&DeleteCallback::Run, base::Unretained(&callback)));
    RunFor(kTimeout);
    EXPECT_TRUE(callback.did_run());
    return callback.num_deleted();
  }

  int DeleteAllForHost(CookieMonster*cm,
                       const GURL& url) {
    DCHECK(cm);
    DeleteCallback callback;
    cm->DeleteAllForHostAsync(
        url, base::Bind(&DeleteCallback::Run, base::Unretained(&callback)));
    RunFor(kTimeout);
    EXPECT_TRUE(callback.did_run());
    return callback.num_deleted();
  }

  bool DeleteCanonicalCookie(CookieMonster*cm,
                             const CookieMonster::CanonicalCookie& cookie) {
    DCHECK(cm);
    SetCookieCallback callback;
    cm->DeleteCanonicalCookieAsync(
        cookie,
        base::Bind(&SetCookieCallback::Run, base::Unretained(&callback)));
    RunFor(kTimeout);
    EXPECT_TRUE(callback.did_run());
    return callback.result();
  }

  void RunFor(int ms) {
    // Runs the test thread message loop for up to ms milliseconds.
    MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        message_loop_factory_.NewRunnableMethod(&MessageLoop::Quit),
        ms);
    MessageLoop::current()->Run();
    message_loop_factory_.RevokeAll();
  }

  // Helper for DeleteAllForHost test; repopulates CM with same layout
  // each time.
  void PopulateCmForDeleteAllForHost(scoped_refptr<CookieMonster> cm) {
    GURL url_top_level_domain_plus_1(kTopLevelDomainPlus1);
    GURL url_top_level_domain_plus_2(kTopLevelDomainPlus2);
    GURL url_top_level_domain_plus_2_secure(kTopLevelDomainPlus2Secure);
    GURL url_top_level_domain_plus_3(kTopLevelDomainPlus3);
    GURL url_other(kOtherDomain);

    DeleteAll(cm);

    // Static population for probe:
    //    * Three levels of domain cookie (.b.a, .c.b.a, .d.c.b.a)
    //    * Three levels of host cookie (w.b.a, w.c.b.a, w.d.c.b.a)
    //    * http_only cookie (w.c.b.a)
    //    * Two secure cookies (.c.b.a, w.c.b.a)
    //    * Two domain path cookies (.c.b.a/dir1, .c.b.a/dir1/dir2)
    //    * Two host path cookies (w.c.b.a/dir1, w.c.b.a/dir1/dir2)

    // Domain cookies
    EXPECT_TRUE(this->SetCookieWithDetails(cm, url_top_level_domain_plus_1,
                                          "dom_1", "X", ".harvard.edu", "/",
                                          base::Time(), false, false));
    EXPECT_TRUE(this->SetCookieWithDetails(cm, url_top_level_domain_plus_2,
                                          "dom_2", "X", ".math.harvard.edu",
                                           "/", base::Time(), false, false));
    EXPECT_TRUE(this->SetCookieWithDetails(cm, url_top_level_domain_plus_3,
                                          "dom_3", "X",
                                          ".bourbaki.math.harvard.edu", "/",
                                          base::Time(), false, false));

    // Host cookies
    EXPECT_TRUE(this->SetCookieWithDetails(cm, url_top_level_domain_plus_1,
                                          "host_1", "X", "", "/",
                                          base::Time(), false, false));
    EXPECT_TRUE(this->SetCookieWithDetails(cm, url_top_level_domain_plus_2,
                                          "host_2", "X", "", "/",
                                          base::Time(), false, false));
    EXPECT_TRUE(this->SetCookieWithDetails(cm, url_top_level_domain_plus_3,
                                          "host_3", "X", "", "/",
                                          base::Time(), false, false));

    // Http_only cookie
    EXPECT_TRUE(this->SetCookieWithDetails(cm, url_top_level_domain_plus_2,
                                          "httpo_check", "X", "", "/",
                                          base::Time(), false, true));

    // Secure cookies
    EXPECT_TRUE(this->SetCookieWithDetails(cm,
                                          url_top_level_domain_plus_2_secure,
                                         "sec_dom", "X", ".math.harvard.edu",
                                         "/", base::Time(), true, false));
    EXPECT_TRUE(this->SetCookieWithDetails(cm,
                                          url_top_level_domain_plus_2_secure,
                                          "sec_host", "X", "", "/",
                                          base::Time(), true, false));

    // Domain path cookies
    EXPECT_TRUE(this->SetCookieWithDetails(cm, url_top_level_domain_plus_2,
                                          "dom_path_1", "X",
                                          ".math.harvard.edu", "/dir1",
                                          base::Time(), false, false));
    EXPECT_TRUE(this->SetCookieWithDetails(cm, url_top_level_domain_plus_2,
                                          "dom_path_2", "X",
                                          ".math.harvard.edu", "/dir1/dir2",
                                          base::Time(), false, false));

    // Host path cookies
    EXPECT_TRUE(this->SetCookieWithDetails(cm, url_top_level_domain_plus_2,
                                          "host_path_1", "X",
                                          "", "/dir1",
                                          base::Time(), false, false));
    EXPECT_TRUE(this->SetCookieWithDetails(cm, url_top_level_domain_plus_2,
                                          "host_path_2", "X",
                                          "", "/dir1/dir2",
                                          base::Time(), false, false));

    EXPECT_EQ(13U, this->GetAllCookies(cm).size());
  }

  Time GetFirstCookieAccessDate(CookieMonster* cm) {
    const CookieList all_cookies(this->GetAllCookies(cm));
    return all_cookies.front().LastAccessDate();
  }

  bool FindAndDeleteCookie(CookieMonster* cm,
                           const std::string& domain,
                           const std::string& name) {
    CookieList cookies = this->GetAllCookies(cm);
    for (CookieList::iterator it = cookies.begin();
         it != cookies.end(); ++it)
      if (it->Domain() == domain && it->Name() == name)
        return this->DeleteCanonicalCookie(cm, *it);
    return false;
  }

  int CountInString(const std::string& str, char c) {
    return std::count(str.begin(), str.end(), c);
  }

  void TestHostGarbageCollectHelper(
      int domain_max_cookies,
      int domain_purge_cookies,
      CookieMonster::ExpiryAndKeyScheme key_scheme) {
    const int more_than_enough_cookies =
        (domain_max_cookies + domain_purge_cookies) * 2;
    // Add a bunch of cookies on a single host, should purge them.
    {
      scoped_refptr<CookieMonster> cm(new CookieMonster(NULL, NULL));
      cm->SetExpiryAndKeyScheme(key_scheme);
      for (int i = 0; i < more_than_enough_cookies; ++i) {
        std::string cookie = base::StringPrintf("a%03d=b", i);
        EXPECT_TRUE(SetCookie(cm, url_google_, cookie));
        std::string cookies = this->GetCookies(cm, url_google_);
        // Make sure we find it in the cookies.
        EXPECT_NE(cookies.find(cookie), std::string::npos);
        // Count the number of cookies.
        EXPECT_LE(CountInString(cookies, '='), domain_max_cookies);
      }
    }

    // Add a bunch of cookies on multiple hosts within a single eTLD.
    // Should keep at least kDomainMaxCookies - kDomainPurgeCookies
    // between them.  If we are using the effective domain keying system
    // (EKS_KEEP_RECENT_AND_PURGE_ETLDP1) we shouldn't go above
    // kDomainMaxCookies for both together.  If we're using the domain
    // keying system (EKS_DISCARD_RECENT_AND_PURGE_DOMAIN), each
    // individual domain shouldn't go above kDomainMaxCookies.
    GURL url_google_specific(kUrlGoogleSpecific);
    {
      scoped_refptr<CookieMonster> cm(new CookieMonster(NULL, NULL));
      cm->SetExpiryAndKeyScheme(key_scheme);
      for (int i = 0; i < more_than_enough_cookies; ++i) {
        std::string cookie_general = base::StringPrintf("a%03d=b", i);
        EXPECT_TRUE(SetCookie(cm, url_google_, cookie_general));
        std::string cookie_specific = base::StringPrintf("c%03d=b", i);
        EXPECT_TRUE(SetCookie(cm, url_google_specific, cookie_specific));
        std::string cookies_general = this->GetCookies(cm, url_google_);
        EXPECT_NE(cookies_general.find(cookie_general), std::string::npos);
        std::string cookies_specific =
            this->GetCookies(cm, url_google_specific);
        EXPECT_NE(cookies_specific.find(cookie_specific), std::string::npos);
        if (key_scheme == CookieMonster::EKS_KEEP_RECENT_AND_PURGE_ETLDP1) {
          EXPECT_LE((CountInString(cookies_general, '=') +
                     CountInString(cookies_specific, '=')),
                    domain_max_cookies);
        } else {
          EXPECT_LE(CountInString(cookies_general, '='), domain_max_cookies);
          EXPECT_LE(CountInString(cookies_specific, '='), domain_max_cookies);
        }
      }
      // After all this, there should be at least
      // kDomainMaxCookies - kDomainPurgeCookies for both URLs.
      std::string cookies_general = this->GetCookies(cm, url_google_);
      std::string cookies_specific = this->GetCookies(cm, url_google_specific);
      if (key_scheme == CookieMonster::EKS_KEEP_RECENT_AND_PURGE_ETLDP1) {
        int total_cookies = (CountInString(cookies_general, '=') +
                             CountInString(cookies_specific, '='));
        EXPECT_GE(total_cookies,
                  domain_max_cookies - domain_purge_cookies);
        EXPECT_LE(total_cookies, domain_max_cookies);
      } else {
        int general_cookies = CountInString(cookies_general, '=');
        int specific_cookies = CountInString(cookies_specific, '=');
        EXPECT_GE(general_cookies,
                  domain_max_cookies - domain_purge_cookies);
        EXPECT_LE(general_cookies, domain_max_cookies);
        EXPECT_GE(specific_cookies,
                  domain_max_cookies - domain_purge_cookies);
        EXPECT_LE(specific_cookies, domain_max_cookies);
      }
    }
  }

  // Function for creating a CM with a number of cookies in it,
  // no store (and hence no ability to affect access time).
  CookieMonster* CreateMonsterForGC(int num_cookies) {
    CookieMonster* cm(new CookieMonster(NULL, NULL));
    for (int i = 0; i < num_cookies; i++) {
      SetCookie(cm, GURL(StringPrintf("http://h%05d.izzle", i)), "a=1");
    }
    return cm;
  }

 protected:
  GURL url_google_;
  GURL url_google_secure_;
  GURL url_google_foo_;
  GURL url_google_bar_;

  ScopedRunnableMethodFactory<MessageLoop> message_loop_factory_;
};

// TODO(erikwright): Replace the other callbacks and synchronous helper methods
// in this test suite with these Mocks.
template<typename T, typename C> class MockCookieCallback {
 public:
  C AsCallback() {
    return base::Bind(&T::Invoke, base::Unretained(static_cast<T*>(this)));
  }
};

class MockGetCookiesCallback
  : public MockCookieCallback<MockGetCookiesCallback,
                              CookieStore::GetCookiesCallback> {
 public:
  MOCK_METHOD1(Invoke, void(const std::string& cookies));
};

class MockGetCookieInfoCallback
  : public MockCookieCallback<MockGetCookieInfoCallback,
                              CookieStore::GetCookieInfoCallback> {
 public:
  MOCK_METHOD2(Invoke,
               void(const std::string& cookies,
                    const std::vector<CookieStore::CookieInfo>& cookie_infos));
};

class MockSetCookiesCallback
  : public MockCookieCallback<MockSetCookiesCallback,
                              CookieStore::SetCookiesCallback> {
 public:
  MOCK_METHOD1(Invoke, void(bool success));
};

class MockClosure
  : public MockCookieCallback<MockClosure, base::Closure> {
 public:
  MOCK_METHOD0(Invoke, void(void));
};

class MockGetCookieListCallback
  : public MockCookieCallback<MockGetCookieListCallback,
                              CookieMonster::GetCookieListCallback> {
 public:
  MOCK_METHOD1(Invoke, void(const CookieList& cookies));
};

class MockDeleteCallback
  : public MockCookieCallback<MockDeleteCallback,
                              CookieMonster::DeleteCallback> {
 public:
  MOCK_METHOD1(Invoke, void(int num_deleted));
};

class MockDeleteCookieCallback
  : public MockCookieCallback<MockDeleteCookieCallback,
                              CookieMonster::DeleteCookieCallback> {
 public:
  MOCK_METHOD1(Invoke, void(bool success));
};

ACTION(QuitCurrentMessageLoop) {
  MessageLoop::current()->PostTask(FROM_HERE, new MessageLoop::QuitTask());
}

// TODO(erikwright): When the synchronous helpers 'GetCookies' etc. are removed,
// rename these, removing the 'Action' suffix.
ACTION_P4(DeleteCookieAction, cookie_monster, url, name, callback) {
  cookie_monster->DeleteCookieAsync(url, name, callback->AsCallback());
}
ACTION_P3(GetCookiesAction, cookie_monster, url, callback) {
  cookie_monster->GetCookiesWithOptionsAsync(
      url, CookieOptions(), callback->AsCallback());
}
ACTION_P3(GetCookiesWithInfoAction, cookie_monster, url, callback) {
  cookie_monster->GetCookiesWithInfoAsync(
      url, CookieOptions(), callback->AsCallback());
}
ACTION_P4(SetCookieAction, cookie_monster, url, cookie_line, callback) {
  cookie_monster->SetCookieWithOptionsAsync(
      url, cookie_line, CookieOptions(), callback->AsCallback());
}
ACTION_P4(DeleteAllCreatedBetweenAction,
          cookie_monster, delete_begin, delete_end, callback) {
  cookie_monster->DeleteAllCreatedBetweenAsync(
      delete_begin, delete_end, callback->AsCallback());
}
ACTION_P10(SetCookieWithDetailsAction,
           cookie_monster, url, name, value, domain, path, expiration_time,
           secure, http_only, callback) {
  cookie_monster->SetCookieWithDetailsAsync(
      url, name, value, domain, path, expiration_time, secure, http_only,
      callback->AsCallback());
}

ACTION_P2(GetAllCookiesAction, cookie_monster, callback) {
  cookie_monster->GetAllCookiesAsync(callback->AsCallback());
}

ACTION_P3(DeleteAllForHostAction, cookie_monster, url, callback) {
  cookie_monster->DeleteAllForHostAsync(url, callback->AsCallback());
}

ACTION_P3(DeleteCanonicalCookieAction, cookie_monster, cookie, callback) {
  cookie_monster->DeleteCanonicalCookieAsync(cookie, callback->AsCallback());
}

ACTION_P2(DeleteAllAction, cookie_monster, callback) {
  cookie_monster->DeleteAllAsync(callback->AsCallback());
}

ACTION_P3(GetAllCookiesForUrlWithOptionsAction, cookie_monster, url, callback) {
  cookie_monster->GetAllCookiesForURLWithOptionsAsync(
      url, CookieOptions(), callback->AsCallback());
}

ACTION_P3(GetAllCookiesForUrlAction, cookie_monster, url, callback) {
  cookie_monster->GetAllCookiesForURLAsync(url, callback->AsCallback());
}

ACTION_P(PushCallbackAction, callback_vector) {
  callback_vector->push(arg1);
}
}  // namespace

// This test suite verifies the task deferral behaviour of the CookieMonster.
// Specifically, for each asynchronous method, verify that:
// 1. invoking it on an uninitialized cookie store causes the store to begin
//    chain-loading its backing data or loading data for a specific domain key
//    (eTLD+1).
// 2. The initial invocation does not complete until the loading completes.
// 3. Invocations after the loading has completed complete immediately.
class DeferredCookieTaskTest : public CookieMonsterTest {
 protected:
  DeferredCookieTaskTest() {
    persistent_store_ = new NewMockPersistentCookieStore();
    cookie_monster_ = new CookieMonster(persistent_store_.get(), NULL);
  }

  // Defines a cookie to be returned from PersistentCookieStore::Load
  void DeclareLoadedCookie(const std::string& key,
                      const std::string& cookie_line,
                      const base::Time& creation_time) {
    AddCookieToList(key, cookie_line, creation_time, &loaded_cookies_);
  }

  // Runs the message loop, waiting until PersistentCookieStore::Load is called.
  // Call CompleteLoadingAndWait to cause the load to complete.
  void WaitForLoadCall() {
    RunFor(kTimeout);

    // Verify that PeristentStore::Load was called.
    testing::Mock::VerifyAndClear(persistent_store_.get());
  }

  // Invokes the PersistentCookieStore::LoadCookiesForKey completion callbacks
  // and PersistentCookieStore::Load completion callback and waits
  // until the message loop is quit.
  void CompleteLoadingAndWait() {
    while (!loaded_for_key_callbacks_.empty()) {
      loaded_for_key_callbacks_.front().Run(loaded_cookies_);
      loaded_cookies_.clear();
      loaded_for_key_callbacks_.pop();
    }

    loaded_callback_.Run(loaded_cookies_);
    RunFor(kTimeout);
  }

  // Performs the provided action, expecting it to cause a call to
  // PersistentCookieStore::Load. Call WaitForLoadCall to verify the load call
  // is received.
  void BeginWith(testing::Action<void(void)> action) {
    EXPECT_CALL(*this, Begin()).WillOnce(action);
    ExpectLoadCall();
    Begin();
  }

  void BeginWithForDomainKey(std::string key,
                             testing::Action<void(void)> action) {
    EXPECT_CALL(*this, Begin()).WillOnce(action);
    ExpectLoadCall();
    ExpectLoadForKeyCall(key, false);
    Begin();
  }

  // Declares an expectation that PersistentCookieStore::Load will be called,
  // saving the provided callback and sending a quit to the message loop.
  void ExpectLoadCall() {
    EXPECT_CALL(*persistent_store_, Load(testing::_)).WillOnce(testing::DoAll(
        testing::SaveArg<0>(&loaded_callback_),
        QuitCurrentMessageLoop()));
  }

  // Declares an expectation that PersistentCookieStore::LoadCookiesForKey
  // will be called, saving the provided callback and sending a quit to the
  // message loop.
  void ExpectLoadForKeyCall(std::string key, bool quit_queue) {
    if (quit_queue)
      EXPECT_CALL(*persistent_store_, LoadCookiesForKey(key, testing::_)).
          WillOnce(testing::DoAll(
                   PushCallbackAction(&loaded_for_key_callbacks_),
                   QuitCurrentMessageLoop()));
    else
      EXPECT_CALL(*persistent_store_, LoadCookiesForKey(key, testing::_)).
          WillOnce(PushCallbackAction(&loaded_for_key_callbacks_));
  }

  // Invokes the initial action.
  MOCK_METHOD0(Begin, void(void));

  // Returns the CookieMonster instance under test.
  CookieMonster& cookie_monster() { return *cookie_monster_; }

 private:
  // Declares that mock expectations in this test suite are strictly ordered.
  testing::InSequence in_sequence_;
  // Holds cookies to be returned from PersistentCookieStore::Load or
  // PersistentCookieStore::LoadCookiesForKey.
  std::vector<CookieMonster::CanonicalCookie*> loaded_cookies_;
  // Stores the callback passed from the CookieMonster to the
  // PersistentCookieStore::Load
  CookieMonster::PersistentCookieStore::LoadedCallback loaded_callback_;
  // Stores the callback passed from the CookieMonster to the
  // PersistentCookieStore::LoadCookiesForKey
  std::queue<CookieMonster::PersistentCookieStore::LoadedCallback>
    loaded_for_key_callbacks_;

  // Stores the CookieMonster under test.
  scoped_refptr<CookieMonster> cookie_monster_;
  // Stores the mock PersistentCookieStore.
  scoped_refptr<NewMockPersistentCookieStore> persistent_store_;
};

TEST_F(DeferredCookieTaskTest, DeferredGetCookies) {
  DeclareLoadedCookie("www.google.izzle",
                      "X=1; path=/; expires=Mon, 18-Apr-22 22:50:14 GMT",
                      Time::Now() + TimeDelta::FromDays(3));

  MockGetCookiesCallback get_cookies_callback;

  BeginWithForDomainKey("google.izzle", GetCookiesAction(
      &cookie_monster(), url_google_, &get_cookies_callback));

  WaitForLoadCall();

  EXPECT_CALL(get_cookies_callback, Invoke("X=1")).WillOnce(
      GetCookiesAction(&cookie_monster(), url_google_, &get_cookies_callback));
  EXPECT_CALL(get_cookies_callback, Invoke("X=1")).WillOnce(
      QuitCurrentMessageLoop());

  CompleteLoadingAndWait();
}

TEST_F(DeferredCookieTaskTest, DeferredGetCookiesWithInfo) {
  DeclareLoadedCookie("www.google.izzle",
                      "X=1; path=/; expires=Mon, 18-Apr-22 22:50:14 GMT",
                      Time::Now() + TimeDelta::FromDays(3));

  MockGetCookieInfoCallback get_cookie_info_callback;

  BeginWithForDomainKey("google.izzle", GetCookiesWithInfoAction(
      &cookie_monster(), url_google_, &get_cookie_info_callback));

  WaitForLoadCall();

  EXPECT_CALL(get_cookie_info_callback, Invoke("X=1", testing::_)).WillOnce(
      GetCookiesWithInfoAction(
          &cookie_monster(), url_google_, &get_cookie_info_callback));
  EXPECT_CALL(get_cookie_info_callback, Invoke("X=1", testing::_)).WillOnce(
      QuitCurrentMessageLoop());

  CompleteLoadingAndWait();
}

TEST_F(DeferredCookieTaskTest, DeferredSetCookie) {
  MockSetCookiesCallback set_cookies_callback;

  BeginWithForDomainKey("google.izzle", SetCookieAction(
      &cookie_monster(), url_google_, "A=B", &set_cookies_callback));

  WaitForLoadCall();

  EXPECT_CALL(set_cookies_callback, Invoke(true)).WillOnce(
      SetCookieAction(
          &cookie_monster(), url_google_, "X=Y", &set_cookies_callback));
  EXPECT_CALL(set_cookies_callback, Invoke(true)).WillOnce(
      QuitCurrentMessageLoop());

  CompleteLoadingAndWait();
}

TEST_F(DeferredCookieTaskTest, DeferredDeleteCookie) {
  MockClosure delete_cookie_callback;

  BeginWithForDomainKey("google.izzle", DeleteCookieAction(
      &cookie_monster(), url_google_, "A", &delete_cookie_callback));

  WaitForLoadCall();

  EXPECT_CALL(delete_cookie_callback, Invoke()).WillOnce(
      DeleteCookieAction(
          &cookie_monster(), url_google_, "X", &delete_cookie_callback));
  EXPECT_CALL(delete_cookie_callback, Invoke()).WillOnce(
      QuitCurrentMessageLoop());

  CompleteLoadingAndWait();
}

TEST_F(DeferredCookieTaskTest, DeferredSetCookieWithDetails) {
  MockSetCookiesCallback set_cookies_callback;

  BeginWithForDomainKey("google.izzle", SetCookieWithDetailsAction(
      &cookie_monster(), url_google_foo_, "A", "B", std::string(), "/foo",
      base::Time(), false, false, &set_cookies_callback));

  WaitForLoadCall();

  EXPECT_CALL(set_cookies_callback, Invoke(true)).WillOnce(
      SetCookieWithDetailsAction(
          &cookie_monster(), url_google_foo_, "A", "B", std::string(), "/foo",
          base::Time(), false, false, &set_cookies_callback));
  EXPECT_CALL(set_cookies_callback, Invoke(true)).WillOnce(
      QuitCurrentMessageLoop());

  CompleteLoadingAndWait();
}

TEST_F(DeferredCookieTaskTest, DeferredGetAllCookies) {
  DeclareLoadedCookie("www.google.izzle",
                      "X=1; path=/; expires=Mon, 18-Apr-22 22:50:14 GMT",
                      Time::Now() + TimeDelta::FromDays(3));

  MockGetCookieListCallback get_cookie_list_callback;

  BeginWith(GetAllCookiesAction(
      &cookie_monster(), &get_cookie_list_callback));

  WaitForLoadCall();

  EXPECT_CALL(get_cookie_list_callback, Invoke(testing::_)).WillOnce(
      GetAllCookiesAction(&cookie_monster(), &get_cookie_list_callback));
  EXPECT_CALL(get_cookie_list_callback, Invoke(testing::_)).WillOnce(
      QuitCurrentMessageLoop());

  CompleteLoadingAndWait();
}

TEST_F(DeferredCookieTaskTest, DeferredGetAllForUrlCookies) {
  DeclareLoadedCookie("www.google.izzle",
                      "X=1; path=/; expires=Mon, 18-Apr-22 22:50:14 GMT",
                      Time::Now() + TimeDelta::FromDays(3));

  MockGetCookieListCallback get_cookie_list_callback;

  BeginWithForDomainKey("google.izzle", GetAllCookiesForUrlAction(
      &cookie_monster(), url_google_, &get_cookie_list_callback));

  WaitForLoadCall();

  EXPECT_CALL(get_cookie_list_callback, Invoke(testing::_)).WillOnce(
      GetAllCookiesForUrlAction(
          &cookie_monster(), url_google_, &get_cookie_list_callback));
  EXPECT_CALL(get_cookie_list_callback, Invoke(testing::_)).WillOnce(
      QuitCurrentMessageLoop());

  CompleteLoadingAndWait();
}


TEST_F(DeferredCookieTaskTest, DeferredGetAllForUrlWithOptionsCookies) {
  DeclareLoadedCookie("www.google.izzle",
                      "X=1; path=/; expires=Mon, 18-Apr-22 22:50:14 GMT",
                      Time::Now() + TimeDelta::FromDays(3));

  MockGetCookieListCallback get_cookie_list_callback;

  BeginWithForDomainKey("google.izzle", GetAllCookiesForUrlWithOptionsAction(
      &cookie_monster(), url_google_, &get_cookie_list_callback));

  WaitForLoadCall();

  EXPECT_CALL(get_cookie_list_callback, Invoke(testing::_)).WillOnce(
      GetAllCookiesForUrlWithOptionsAction(
          &cookie_monster(), url_google_, &get_cookie_list_callback));
  EXPECT_CALL(get_cookie_list_callback, Invoke(testing::_)).WillOnce(
      QuitCurrentMessageLoop());

  CompleteLoadingAndWait();
}

TEST_F(DeferredCookieTaskTest, DeferredDeleteAllCookies) {
  MockDeleteCallback delete_callback;

  BeginWith(DeleteAllAction(
      &cookie_monster(), &delete_callback));

  WaitForLoadCall();

  EXPECT_CALL(delete_callback, Invoke(false)).WillOnce(
      DeleteAllAction(&cookie_monster(), &delete_callback));
  EXPECT_CALL(delete_callback, Invoke(false)).WillOnce(
      QuitCurrentMessageLoop());

  CompleteLoadingAndWait();
}

TEST_F(DeferredCookieTaskTest, DeferredDeleteAllCreatedBetweenCookies) {
  MockDeleteCallback delete_callback;

  BeginWith(DeleteAllCreatedBetweenAction(
      &cookie_monster(), base::Time(), base::Time::Now(), &delete_callback));

  WaitForLoadCall();

  EXPECT_CALL(delete_callback, Invoke(false)).WillOnce(
      DeleteAllCreatedBetweenAction(
          &cookie_monster(), base::Time(), base::Time::Now(),
          &delete_callback));
  EXPECT_CALL(delete_callback, Invoke(false)).WillOnce(
      QuitCurrentMessageLoop());

  CompleteLoadingAndWait();
}

TEST_F(DeferredCookieTaskTest, DeferredDeleteAllForHostCookies) {
  MockDeleteCallback delete_callback;

  BeginWithForDomainKey("google.izzle", DeleteAllForHostAction(
      &cookie_monster(), url_google_, &delete_callback));

  WaitForLoadCall();

  EXPECT_CALL(delete_callback, Invoke(false)).WillOnce(
      DeleteAllForHostAction(
          &cookie_monster(), url_google_, &delete_callback));
  EXPECT_CALL(delete_callback, Invoke(false)).WillOnce(
      QuitCurrentMessageLoop());

  CompleteLoadingAndWait();
}

TEST_F(DeferredCookieTaskTest, DeferredDeleteCanonicalCookie) {
  std::vector<CookieMonster::CanonicalCookie*> cookies;
  CookieMonster::CanonicalCookie cookie = BuildCanonicalCookie(
      "www.google.com", "X=1; path=/", base::Time::Now());

  MockDeleteCookieCallback delete_cookie_callback;

  BeginWith(DeleteCanonicalCookieAction(
      &cookie_monster(), cookie, &delete_cookie_callback));

  WaitForLoadCall();

  EXPECT_CALL(delete_cookie_callback, Invoke(false)).WillOnce(
      DeleteCanonicalCookieAction(
      &cookie_monster(), cookie, &delete_cookie_callback));
  EXPECT_CALL(delete_cookie_callback, Invoke(false)).WillOnce(
      QuitCurrentMessageLoop());

  CompleteLoadingAndWait();
}

// Verify that a series of queued tasks are executed in order upon loading of
// the backing store and that new tasks received while the queued tasks are
// being dispatched go to the end of the queue.
TEST_F(DeferredCookieTaskTest, DeferredTaskOrder) {
  DeclareLoadedCookie("www.google.izzle",
                      "X=1; path=/; expires=Mon, 18-Apr-22 22:50:14 GMT",
                      Time::Now() + TimeDelta::FromDays(3));

  MockGetCookiesCallback get_cookies_callback;
  MockSetCookiesCallback set_cookies_callback;
  MockClosure delete_cookie_callback;
  MockGetCookieInfoCallback get_cookie_info_callback;

  EXPECT_CALL(*this, Begin()).WillOnce(testing::DoAll(
      GetCookiesAction(
          &cookie_monster(), url_google_, &get_cookies_callback),
      SetCookieAction(
          &cookie_monster(), url_google_, "A=B", &set_cookies_callback),
      DeleteCookieAction(
          &cookie_monster(), url_google_, "A", &delete_cookie_callback)));
  ExpectLoadCall();
  ExpectLoadForKeyCall("google.izzle", false);
  Begin();

  WaitForLoadCall();
  EXPECT_CALL(get_cookies_callback, Invoke("X=1")).WillOnce(
      GetCookiesWithInfoAction(
          &cookie_monster(), url_google_, &get_cookie_info_callback));
  EXPECT_CALL(get_cookie_info_callback, Invoke("X=1", testing::_)).WillOnce(
      QuitCurrentMessageLoop());
  EXPECT_CALL(set_cookies_callback, Invoke(true));
  EXPECT_CALL(delete_cookie_callback, Invoke());

  CompleteLoadingAndWait();
}

TEST_F(CookieMonsterTest, DomainTest) {
  scoped_refptr<MockPersistentCookieStore> store(
      new MockPersistentCookieStore);
  scoped_refptr<CookieMonster> cm(new CookieMonster(store, NULL));
  EXPECT_TRUE(SetCookie(cm, url_google_, "A=B"));
  EXPECT_EQ("A=B", GetCookies(cm, url_google_));
  EXPECT_TRUE(SetCookie(cm, url_google_, "C=D; domain=.google.izzle"));
  EXPECT_EQ("A=B; C=D", GetCookies(cm, url_google_));

  // Verify that A=B was set as a host cookie rather than a domain
  // cookie -- should not be accessible from a sub sub-domain.
  EXPECT_EQ("C=D", GetCookies(cm, GURL("http://foo.www.google.izzle")));

  // Test and make sure we find domain cookies on the same domain.
  EXPECT_TRUE(SetCookie(cm, url_google_, "E=F; domain=.www.google.izzle"));
  EXPECT_EQ("A=B; C=D; E=F", GetCookies(cm, url_google_));

  // Test setting a domain= that doesn't start w/ a dot, should
  // treat it as a domain cookie, as if there was a pre-pended dot.
  EXPECT_TRUE(SetCookie(cm, url_google_, "G=H; domain=www.google.izzle"));
  EXPECT_EQ("A=B; C=D; E=F; G=H", GetCookies(cm, url_google_));

  // Test domain enforcement, should fail on a sub-domain or something too deep.
  EXPECT_FALSE(SetCookie(cm, url_google_, "I=J; domain=.izzle"));
  EXPECT_EQ("", GetCookies(cm, GURL("http://a.izzle")));
  EXPECT_FALSE(SetCookie(cm, url_google_, "K=L; domain=.bla.www.google.izzle"));
  EXPECT_EQ("C=D; E=F; G=H",
            GetCookies(cm, GURL("http://bla.www.google.izzle")));
  EXPECT_EQ("A=B; C=D; E=F; G=H", GetCookies(cm, url_google_));

  // Nothing was persisted to the backing store.
  EXPECT_EQ(0u, store->commands().size());
}

// FireFox recognizes domains containing trailing periods as valid.
// IE and Safari do not. Assert the expected policy here.
TEST_F(CookieMonsterTest, DomainWithTrailingDotTest) {
  scoped_refptr<MockPersistentCookieStore> store(
      new MockPersistentCookieStore);
  scoped_refptr<CookieMonster> cm(new CookieMonster(store, NULL));
  EXPECT_FALSE(SetCookie(cm, url_google_, "a=1; domain=.www.google.com."));
  EXPECT_FALSE(SetCookie(cm, url_google_, "b=2; domain=.www.google.com.."));
  EXPECT_EQ("", GetCookies(cm, url_google_));

  // Nothing was persisted to the backing store.
  EXPECT_EQ(0u, store->commands().size());
}

// Test that cookies can bet set on higher level domains.
// http://b/issue?id=896491
TEST_F(CookieMonsterTest, ValidSubdomainTest) {
  scoped_refptr<MockPersistentCookieStore> store(
      new MockPersistentCookieStore);
  scoped_refptr<CookieMonster> cm(new CookieMonster(store, NULL));
  GURL url_abcd("http://a.b.c.d.com");
  GURL url_bcd("http://b.c.d.com");
  GURL url_cd("http://c.d.com");
  GURL url_d("http://d.com");

  EXPECT_TRUE(SetCookie(cm, url_abcd, "a=1; domain=.a.b.c.d.com"));
  EXPECT_TRUE(SetCookie(cm, url_abcd, "b=2; domain=.b.c.d.com"));
  EXPECT_TRUE(SetCookie(cm, url_abcd, "c=3; domain=.c.d.com"));
  EXPECT_TRUE(SetCookie(cm, url_abcd, "d=4; domain=.d.com"));

  EXPECT_EQ("a=1; b=2; c=3; d=4", GetCookies(cm, url_abcd));
  EXPECT_EQ("b=2; c=3; d=4", GetCookies(cm, url_bcd));
  EXPECT_EQ("c=3; d=4", GetCookies(cm, url_cd));
  EXPECT_EQ("d=4", GetCookies(cm, url_d));

  // Check that the same cookie can exist on different sub-domains.
  EXPECT_TRUE(SetCookie(cm, url_bcd, "X=bcd; domain=.b.c.d.com"));
  EXPECT_TRUE(SetCookie(cm, url_bcd, "X=cd; domain=.c.d.com"));
  EXPECT_EQ("b=2; c=3; d=4; X=bcd; X=cd", GetCookies(cm, url_bcd));
  EXPECT_EQ("c=3; d=4; X=cd", GetCookies(cm, url_cd));

  // Nothing was persisted to the backing store.
  EXPECT_EQ(0u, store->commands().size());
}

// Test that setting a cookie which specifies an invalid domain has
// no side-effect. An invalid domain in this context is one which does
// not match the originating domain.
// http://b/issue?id=896472
TEST_F(CookieMonsterTest, InvalidDomainTest) {
  {
    scoped_refptr<MockPersistentCookieStore> store(
        new MockPersistentCookieStore);

    scoped_refptr<CookieMonster> cm(new CookieMonster(store, NULL));
    GURL url_foobar("http://foo.bar.com");

    // More specific sub-domain than allowed.
    EXPECT_FALSE(SetCookie(cm, url_foobar, "a=1; domain=.yo.foo.bar.com"));

    EXPECT_FALSE(SetCookie(cm, url_foobar, "b=2; domain=.foo.com"));
    EXPECT_FALSE(SetCookie(cm, url_foobar, "c=3; domain=.bar.foo.com"));

    // Different TLD, but the rest is a substring.
    EXPECT_FALSE(SetCookie(cm, url_foobar, "d=4; domain=.foo.bar.com.net"));

    // A substring that isn't really a parent domain.
    EXPECT_FALSE(SetCookie(cm, url_foobar, "e=5; domain=ar.com"));

    // Completely invalid domains:
    EXPECT_FALSE(SetCookie(cm, url_foobar, "f=6; domain=."));
    EXPECT_FALSE(SetCookie(cm, url_foobar, "g=7; domain=/"));
    EXPECT_FALSE(SetCookie(cm, url_foobar, "h=8; domain=http://foo.bar.com"));
    EXPECT_FALSE(SetCookie(cm, url_foobar, "i=9; domain=..foo.bar.com"));
    EXPECT_FALSE(SetCookie(cm, url_foobar, "j=10; domain=..bar.com"));

    // Make sure there isn't something quirky in the domain canonicalization
    // that supports full URL semantics.
    EXPECT_FALSE(SetCookie(cm, url_foobar, "k=11; domain=.foo.bar.com?blah"));
    EXPECT_FALSE(SetCookie(cm, url_foobar, "l=12; domain=.foo.bar.com/blah"));
    EXPECT_FALSE(SetCookie(cm, url_foobar, "m=13; domain=.foo.bar.com:80"));
    EXPECT_FALSE(SetCookie(cm, url_foobar, "n=14; domain=.foo.bar.com:"));
    EXPECT_FALSE(SetCookie(cm, url_foobar, "o=15; domain=.foo.bar.com#sup"));

    EXPECT_EQ("", GetCookies(cm, url_foobar));

    // Nothing was persisted to the backing store.
    EXPECT_EQ(0u, store->commands().size());
  }

  {
    // Make sure the cookie code hasn't gotten its subdomain string handling
    // reversed, missed a suffix check, etc.  It's important here that the two
    // hosts below have the same domain + registry.
    scoped_refptr<CookieMonster> cm(new CookieMonster(NULL, NULL));
    GURL url_foocom("http://foo.com.com");
    EXPECT_FALSE(SetCookie(cm, url_foocom, "a=1; domain=.foo.com.com.com"));
    EXPECT_EQ("", GetCookies(cm, url_foocom));
  }
}

// Test the behavior of omitting dot prefix from domain, should
// function the same as FireFox.
// http://b/issue?id=889898
TEST_F(CookieMonsterTest, DomainWithoutLeadingDotTest) {
  {  // The omission of dot results in setting a domain cookie.
    scoped_refptr<CookieMonster> cm(new CookieMonster(NULL, NULL));
    GURL url_hosted("http://manage.hosted.filefront.com");
    GURL url_filefront("http://www.filefront.com");
    EXPECT_TRUE(SetCookie(cm, url_hosted, "sawAd=1; domain=filefront.com"));
    EXPECT_EQ("sawAd=1", GetCookies(cm, url_hosted));
    EXPECT_EQ("sawAd=1", GetCookies(cm, url_filefront));
  }

  {  // Even when the domains match exactly, don't consider it host cookie.
    scoped_refptr<CookieMonster> cm(new CookieMonster(NULL, NULL));
    GURL url("http://www.google.com");
    EXPECT_TRUE(SetCookie(cm, url, "a=1; domain=www.google.com"));
    EXPECT_EQ("a=1", GetCookies(cm, url));
    EXPECT_EQ("a=1", GetCookies(cm, GURL("http://sub.www.google.com")));
    EXPECT_EQ("", GetCookies(cm, GURL("http://something-else.com")));
  }
}

// Test that the domain specified in cookie string is treated case-insensitive
// http://b/issue?id=896475.
TEST_F(CookieMonsterTest, CaseInsensitiveDomainTest) {
  scoped_refptr<CookieMonster> cm(new CookieMonster(NULL, NULL));
  GURL url("http://www.google.com");
  EXPECT_TRUE(SetCookie(cm, url, "a=1; domain=.GOOGLE.COM"));
  EXPECT_TRUE(SetCookie(cm, url, "b=2; domain=.wWw.gOOgLE.coM"));
  EXPECT_EQ("a=1; b=2", GetCookies(cm, url));
}

TEST_F(CookieMonsterTest, TestIpAddress) {
  GURL url_ip("http://1.2.3.4/weee");
  {
    scoped_refptr<CookieMonster> cm(new CookieMonster(NULL, NULL));
    EXPECT_TRUE(SetCookie(cm, url_ip, kValidCookieLine));
    EXPECT_EQ("A=B", GetCookies(cm, url_ip));
  }

  {  // IP addresses should not be able to set domain cookies.
    scoped_refptr<CookieMonster> cm(new CookieMonster(NULL, NULL));
    EXPECT_FALSE(SetCookie(cm, url_ip, "b=2; domain=.1.2.3.4"));
    EXPECT_FALSE(SetCookie(cm, url_ip, "c=3; domain=.3.4"));
    EXPECT_EQ("", GetCookies(cm, url_ip));
    // It should be allowed to set a cookie if domain= matches the IP address
    // exactly.  This matches IE/Firefox, even though it seems a bit wrong.
    EXPECT_FALSE(SetCookie(cm, url_ip, "b=2; domain=1.2.3.3"));
    EXPECT_EQ("", GetCookies(cm, url_ip));
    EXPECT_TRUE(SetCookie(cm, url_ip, "b=2; domain=1.2.3.4"));
    EXPECT_EQ("b=2", GetCookies(cm, url_ip));
  }
}

// Test host cookies, and setting of cookies on TLD.
TEST_F(CookieMonsterTest, TestNonDottedAndTLD) {
  {
    scoped_refptr<CookieMonster> cm(new CookieMonster(NULL, NULL));
    GURL url("http://com/");
    // Allow setting on "com", (but only as a host cookie).
    EXPECT_TRUE(SetCookie(cm, url, "a=1"));
    EXPECT_FALSE(SetCookie(cm, url, "b=2; domain=.com"));
    EXPECT_FALSE(SetCookie(cm, url, "c=3; domain=com"));
    EXPECT_EQ("a=1", GetCookies(cm, url));
    // Make sure it doesn't show up for a normal .com, it should be a host
    // not a domain cookie.
    EXPECT_EQ("", GetCookies(cm, GURL("http://hopefully-no-cookies.com/")));
    EXPECT_EQ("", GetCookies(cm, GURL("http://.com/")));
  }

  {  // http://com. should be treated the same as http://com.
    scoped_refptr<CookieMonster> cm(new CookieMonster(NULL, NULL));
    GURL url("http://com./index.html");
    EXPECT_TRUE(SetCookie(cm, url, "a=1"));
    EXPECT_EQ("a=1", GetCookies(cm, url));
    EXPECT_EQ("", GetCookies(cm, GURL("http://hopefully-no-cookies.com./")));
  }

  {  // Should not be able to set host cookie from a subdomain.
    scoped_refptr<CookieMonster> cm(new CookieMonster(NULL, NULL));
    GURL url("http://a.b");
    EXPECT_FALSE(SetCookie(cm, url, "a=1; domain=.b"));
    EXPECT_FALSE(SetCookie(cm, url, "b=2; domain=b"));
    EXPECT_EQ("", GetCookies(cm, url));
  }

  {  // Same test as above, but explicitly on a known TLD (com).
    scoped_refptr<CookieMonster> cm(new CookieMonster(NULL, NULL));
    GURL url("http://google.com");
    EXPECT_FALSE(SetCookie(cm, url, "a=1; domain=.com"));
    EXPECT_FALSE(SetCookie(cm, url, "b=2; domain=com"));
    EXPECT_EQ("", GetCookies(cm, url));
  }

  {  // Make sure can't set cookie on TLD which is dotted.
    scoped_refptr<CookieMonster> cm(new CookieMonster(NULL, NULL));
    GURL url("http://google.co.uk");
    EXPECT_FALSE(SetCookie(cm, url, "a=1; domain=.co.uk"));
    EXPECT_FALSE(SetCookie(cm, url, "b=2; domain=.uk"));
    EXPECT_EQ("", GetCookies(cm, url));
    EXPECT_EQ("", GetCookies(cm, GURL("http://something-else.co.uk")));
    EXPECT_EQ("", GetCookies(cm, GURL("http://something-else.uk")));
  }

  {  // Intranet URLs should only be able to set host cookies.
    scoped_refptr<CookieMonster> cm(new CookieMonster(NULL, NULL));
    GURL url("http://b");
    EXPECT_TRUE(SetCookie(cm, url, "a=1"));
    EXPECT_FALSE(SetCookie(cm, url, "b=2; domain=.b"));
    EXPECT_FALSE(SetCookie(cm, url, "c=3; domain=b"));
    EXPECT_EQ("a=1", GetCookies(cm, url));
  }
}

// Test reading/writing cookies when the domain ends with a period,
// as in "www.google.com."
TEST_F(CookieMonsterTest, TestHostEndsWithDot) {
  scoped_refptr<CookieMonster> cm(new CookieMonster(NULL, NULL));
  GURL url("http://www.google.com");
  GURL url_with_dot("http://www.google.com.");
  EXPECT_TRUE(SetCookie(cm, url, "a=1"));
  EXPECT_EQ("a=1", GetCookies(cm, url));

  // Do not share cookie space with the dot version of domain.
  // Note: this is not what FireFox does, but it _is_ what IE+Safari do.
  EXPECT_FALSE(SetCookie(cm, url, "b=2; domain=.www.google.com."));
  EXPECT_EQ("a=1", GetCookies(cm, url));

  EXPECT_TRUE(SetCookie(cm, url_with_dot, "b=2; domain=.google.com."));
  EXPECT_EQ("b=2", GetCookies(cm, url_with_dot));

  // Make sure there weren't any side effects.
  EXPECT_EQ(GetCookies(cm, GURL("http://hopefully-no-cookies.com/")), "");
  EXPECT_EQ("", GetCookies(cm, GURL("http://.com/")));
}

TEST_F(CookieMonsterTest, InvalidScheme) {
  scoped_refptr<CookieMonster> cm(new CookieMonster(NULL, NULL));
  EXPECT_FALSE(SetCookie(cm, GURL(kUrlFtp), kValidCookieLine));
}

TEST_F(CookieMonsterTest, InvalidScheme_Read) {
  scoped_refptr<CookieMonster> cm(new CookieMonster(NULL, NULL));
  EXPECT_TRUE(SetCookie(cm, GURL(kUrlGoogle), kValidDomainCookieLine));
  EXPECT_EQ("", GetCookies(cm, GURL(kUrlFtp)));
}

TEST_F(CookieMonsterTest, PathTest) {
  std::string url("http://www.google.izzle");
  scoped_refptr<CookieMonster> cm(new CookieMonster(NULL, NULL));
  EXPECT_TRUE(SetCookie(cm, GURL(url), "A=B; path=/wee"));
  EXPECT_EQ("A=B", GetCookies(cm, GURL(url + "/wee")));
  EXPECT_EQ("A=B", GetCookies(cm, GURL(url + "/wee/")));
  EXPECT_EQ("A=B", GetCookies(cm, GURL(url + "/wee/war")));
  EXPECT_EQ("A=B", GetCookies(cm, GURL(url + "/wee/war/more/more")));
  EXPECT_EQ("", GetCookies(cm, GURL(url + "/weehee")));
  EXPECT_EQ("", GetCookies(cm, GURL(url + "/")));

  // If we add a 0 length path, it should default to /
  EXPECT_TRUE(SetCookie(cm, GURL(url), "A=C; path="));
  EXPECT_EQ("A=B; A=C", GetCookies(cm, GURL(url + "/wee")));
  EXPECT_EQ("A=C", GetCookies(cm, GURL(url + "/")));
}

TEST_F(CookieMonsterTest, HttpOnlyTest) {
  scoped_refptr<CookieMonster> cm(new CookieMonster(NULL, NULL));
  CookieOptions options;
  options.set_include_httponly();

  // Create a httponly cookie.
  EXPECT_TRUE(SetCookieWithOptions(cm, url_google_, "A=B; httponly", options));

  // Check httponly read protection.
  EXPECT_EQ("", GetCookies(cm, url_google_));
  EXPECT_EQ("A=B", GetCookiesWithOptions(cm, url_google_, options));

  // Check httponly overwrite protection.
  EXPECT_FALSE(SetCookie(cm, url_google_, "A=C"));
  EXPECT_EQ("", GetCookies(cm, url_google_));
  EXPECT_EQ("A=B", GetCookiesWithOptions(cm, url_google_, options));
  EXPECT_TRUE(SetCookieWithOptions(cm, url_google_, "A=C", options));
  EXPECT_EQ("A=C", GetCookies(cm, url_google_));

  // Check httponly create protection.
  EXPECT_FALSE(SetCookie(cm, url_google_, "B=A; httponly"));
  EXPECT_EQ("A=C", GetCookiesWithOptions(cm, url_google_, options));
  EXPECT_TRUE(SetCookieWithOptions(cm, url_google_, "B=A; httponly", options));
  EXPECT_EQ("A=C; B=A", GetCookiesWithOptions(cm, url_google_, options));
  EXPECT_EQ("A=C", GetCookies(cm, url_google_));
}

TEST_F(CookieMonsterTest, GetCookiesWithInfo) {
  scoped_refptr<CookieMonster> cm(new CookieMonster(NULL, NULL));
  CookieOptions options;

  EXPECT_TRUE(SetCookieWithOptions(cm, url_google_, "A=B", options));
  EXPECT_TRUE(SetCookieWithOptions(cm, url_google_,
    "C=D; Mac-Key=390jfn0awf3; Mac-Algorithm=hmac-sha-1", options));

  EXPECT_EQ("A=B; C=D", GetCookiesWithOptions(cm, url_google_, options));

  std::string cookie_line;
  std::vector<CookieStore::CookieInfo> cookie_infos;

  GetCookiesWithInfo(cm, url_google_, options, &cookie_line, &cookie_infos);

  EXPECT_EQ("A=B; C=D", cookie_line);

  EXPECT_EQ(2U, cookie_infos.size());

  EXPECT_EQ("A", cookie_infos[0].name);
  EXPECT_EQ("", cookie_infos[0].mac_key);
  EXPECT_EQ("", cookie_infos[0].mac_algorithm);

  EXPECT_EQ("C", cookie_infos[1].name);
  EXPECT_EQ("390jfn0awf3", cookie_infos[1].mac_key);
  EXPECT_EQ("hmac-sha-1", cookie_infos[1].mac_algorithm);
}

namespace {

struct CookieDateParsingCase {
  const char* str;
  const bool valid;
  const time_t epoch;
};

struct DomainIsHostOnlyCase {
  const char* str;
  const bool is_host_only;
};

}  // namespace

TEST_F(CookieMonsterTest, TestCookieDateParsing) {
  const CookieDateParsingCase tests[] = {
    { "Sat, 15-Apr-17 21:01:22 GMT",           true, 1492290082 },
    { "Thu, 19-Apr-2007 16:00:00 GMT",         true, 1176998400 },
    { "Wed, 25 Apr 2007 21:02:13 GMT",         true, 1177534933 },
    { "Thu, 19/Apr\\2007 16:00:00 GMT",        true, 1176998400 },
    { "Fri, 1 Jan 2010 01:01:50 GMT",          true, 1262307710 },
    { "Wednesday, 1-Jan-2003 00:00:00 GMT",    true, 1041379200 },
    { ", 1-Jan-2003 00:00:00 GMT",             true, 1041379200 },
    { " 1-Jan-2003 00:00:00 GMT",              true, 1041379200 },
    { "1-Jan-2003 00:00:00 GMT",               true, 1041379200 },
    { "Wed,18-Apr-07 22:50:12 GMT",            true, 1176936612 },
    { "WillyWonka  , 18-Apr-07 22:50:12 GMT",  true, 1176936612 },
    { "WillyWonka  , 18-Apr-07 22:50:12",      true, 1176936612 },
    { "WillyWonka  ,  18-apr-07   22:50:12",   true, 1176936612 },
    { "Mon, 18-Apr-1977 22:50:13 GMT",         true, 230251813 },
    { "Mon, 18-Apr-77 22:50:13 GMT",           true, 230251813 },
    // If the cookie came in with the expiration quoted (which in terms of
    // the RFC you shouldn't do), we will get string quoted.  Bug 1261605.
    { "\"Sat, 15-Apr-17\\\"21:01:22\\\"GMT\"", true, 1492290082 },
    // Test with full month names and partial names.
    { "Partyday, 18- April-07 22:50:12",       true, 1176936612 },
    { "Partyday, 18 - Apri-07 22:50:12",       true, 1176936612 },
    { "Wednes, 1-Januar-2003 00:00:00 GMT",    true, 1041379200 },
    // Test that we always take GMT even with other time zones or bogus
    // values.  The RFC says everything should be GMT, and in the worst case
    // we are 24 hours off because of zone issues.
    { "Sat, 15-Apr-17 21:01:22",               true, 1492290082 },
    { "Sat, 15-Apr-17 21:01:22 GMT-2",         true, 1492290082 },
    { "Sat, 15-Apr-17 21:01:22 GMT BLAH",      true, 1492290082 },
    { "Sat, 15-Apr-17 21:01:22 GMT-0400",      true, 1492290082 },
    { "Sat, 15-Apr-17 21:01:22 GMT-0400 (EDT)",true, 1492290082 },
    { "Sat, 15-Apr-17 21:01:22 DST",           true, 1492290082 },
    { "Sat, 15-Apr-17 21:01:22 -0400",         true, 1492290082 },
    { "Sat, 15-Apr-17 21:01:22 (hello there)", true, 1492290082 },
    // Test that if we encounter multiple : fields, that we take the first
    // that correctly parses.
    { "Sat, 15-Apr-17 21:01:22 11:22:33",      true, 1492290082 },
    { "Sat, 15-Apr-17 ::00 21:01:22",          true, 1492290082 },
    { "Sat, 15-Apr-17 boink:z 21:01:22",       true, 1492290082 },
    // We take the first, which in this case is invalid.
    { "Sat, 15-Apr-17 91:22:33 21:01:22",      false, 0 },
    // amazon.com formats their cookie expiration like this.
    { "Thu Apr 18 22:50:12 2007 GMT",          true, 1176936612 },
    // Test that hh:mm:ss can occur anywhere.
    { "22:50:12 Thu Apr 18 2007 GMT",          true, 1176936612 },
    { "Thu 22:50:12 Apr 18 2007 GMT",          true, 1176936612 },
    { "Thu Apr 22:50:12 18 2007 GMT",          true, 1176936612 },
    { "Thu Apr 18 22:50:12 2007 GMT",          true, 1176936612 },
    { "Thu Apr 18 2007 22:50:12 GMT",          true, 1176936612 },
    { "Thu Apr 18 2007 GMT 22:50:12",          true, 1176936612 },
    // Test that the day and year can be anywhere if they are unambigious.
    { "Sat, 15-Apr-17 21:01:22 GMT",           true, 1492290082 },
    { "15-Sat, Apr-17 21:01:22 GMT",           true, 1492290082 },
    { "15-Sat, Apr 21:01:22 GMT 17",           true, 1492290082 },
    { "15-Sat, Apr 21:01:22 GMT 2017",         true, 1492290082 },
    { "15 Apr 21:01:22 2017",                  true, 1492290082 },
    { "15 17 Apr 21:01:22",                    true, 1492290082 },
    { "Apr 15 17 21:01:22",                    true, 1492290082 },
    { "Apr 15 21:01:22 17",                    true, 1492290082 },
    { "2017 April 15 21:01:22",                true, 1492290082 },
    { "15 April 2017 21:01:22",                true, 1492290082 },
    // Some invalid dates
    { "98 April 17 21:01:22",                    false, 0 },
    { "Thu, 012-Aug-2008 20:49:07 GMT",          false, 0 },
    { "Thu, 12-Aug-31841 20:49:07 GMT",          false, 0 },
    { "Thu, 12-Aug-9999999999 20:49:07 GMT",     false, 0 },
    { "Thu, 999999999999-Aug-2007 20:49:07 GMT", false, 0 },
    { "Thu, 12-Aug-2007 20:61:99999999999 GMT",  false, 0 },
    { "IAintNoDateFool",                         false, 0 },
  };

  Time parsed_time;
  for (size_t i = 0; i < arraysize(tests); ++i) {
    parsed_time = CookieMonster::ParseCookieTime(tests[i].str);
    if (!tests[i].valid) {
      EXPECT_FALSE(!parsed_time.is_null()) << tests[i].str;
      continue;
    }
    EXPECT_TRUE(!parsed_time.is_null()) << tests[i].str;
    EXPECT_EQ(tests[i].epoch, parsed_time.ToTimeT()) << tests[i].str;
  }
}

TEST_F(CookieMonsterTest, TestDomainIsHostOnly) {
  const DomainIsHostOnlyCase tests[] = {
    { "",               true },
    { "www.google.com", true },
    { ".google.com",    false }
  };

  for (size_t i = 0; i < arraysize(tests); ++i) {
    EXPECT_EQ(tests[i].is_host_only,
              cookie_util::DomainIsHostOnly(tests[i].str));
  }
}

TEST_F(CookieMonsterTest, TestCookieDeletion) {
  scoped_refptr<MockPersistentCookieStore> store(
      new MockPersistentCookieStore);
  scoped_refptr<CookieMonster> cm(new CookieMonster(store, NULL));

  // Create a session cookie.
  EXPECT_TRUE(SetCookie(cm, url_google_, kValidCookieLine));
  EXPECT_EQ("A=B", GetCookies(cm, url_google_));
  // Delete it via Max-Age.
  EXPECT_TRUE(SetCookie(cm, url_google_,
                           std::string(kValidCookieLine) + "; max-age=0"));
  EXPECT_EQ("", GetCookies(cm, url_google_));

  // Create a session cookie.
  EXPECT_TRUE(SetCookie(cm, url_google_, kValidCookieLine));
  EXPECT_EQ("A=B", GetCookies(cm, url_google_));
  // Delete it via Expires.
  EXPECT_TRUE(SetCookie(cm, url_google_,
                           std::string(kValidCookieLine) +
                           "; expires=Mon, 18-Apr-1977 22:50:13 GMT"));
  EXPECT_EQ("", GetCookies(cm, url_google_));

  // Create a persistent cookie.
  EXPECT_TRUE(SetCookie(cm, url_google_,
                           std::string(kValidCookieLine) +
                           "; expires=Mon, 18-Apr-22 22:50:13 GMT"));
  ASSERT_EQ(1u, store->commands().size());
  EXPECT_EQ(CookieStoreCommand::ADD, store->commands()[0].type);

  EXPECT_EQ("A=B", GetCookies(cm, url_google_));
  // Delete it via Max-Age.
  EXPECT_TRUE(SetCookie(cm, url_google_,
                           std::string(kValidCookieLine) + "; max-age=0"));
  ASSERT_EQ(2u, store->commands().size());
  EXPECT_EQ(CookieStoreCommand::REMOVE, store->commands()[1].type);
  EXPECT_EQ("", GetCookies(cm, url_google_));

  // Create a persistent cookie.
  EXPECT_TRUE(SetCookie(cm, url_google_,
                        std::string(kValidCookieLine) +
                        "; expires=Mon, 18-Apr-22 22:50:13 GMT"));
  ASSERT_EQ(3u, store->commands().size());
  EXPECT_EQ(CookieStoreCommand::ADD, store->commands()[2].type);
  EXPECT_EQ("A=B", GetCookies(cm, url_google_));
  // Delete it via Expires.
  EXPECT_TRUE(SetCookie(cm, url_google_,
                        std::string(kValidCookieLine) +
                        "; expires=Mon, 18-Apr-1977 22:50:13 GMT"));
  ASSERT_EQ(4u, store->commands().size());
  EXPECT_EQ(CookieStoreCommand::REMOVE, store->commands()[3].type);
  EXPECT_EQ("", GetCookies(cm, url_google_));

  // Create a persistent cookie.
  EXPECT_TRUE(SetCookie(cm, url_google_,
                        std::string(kValidCookieLine) +
                        "; expires=Mon, 18-Apr-22 22:50:13 GMT"));
  ASSERT_EQ(5u, store->commands().size());
  EXPECT_EQ(CookieStoreCommand::ADD, store->commands()[4].type);
  EXPECT_EQ("A=B", GetCookies(cm, url_google_));
  // Delete it via Expires, with a unix epoch of 0.
  EXPECT_TRUE(SetCookie(cm, url_google_,
                        std::string(kValidCookieLine) +
                        "; expires=Thu, 1-Jan-1970 00:00:00 GMT"));
  ASSERT_EQ(6u, store->commands().size());
  EXPECT_EQ(CookieStoreCommand::REMOVE, store->commands()[5].type);
  EXPECT_EQ("", GetCookies(cm, url_google_));
}

TEST_F(CookieMonsterTest, TestCookieDeleteAll) {
  scoped_refptr<MockPersistentCookieStore> store(
      new MockPersistentCookieStore);
  scoped_refptr<CookieMonster> cm(new CookieMonster(store, NULL));
  CookieOptions options;
  options.set_include_httponly();

  EXPECT_TRUE(SetCookie(cm, url_google_, kValidCookieLine));
  EXPECT_EQ("A=B", GetCookies(cm, url_google_));

  EXPECT_TRUE(SetCookieWithOptions(cm, url_google_, "C=D; httponly", options));
  EXPECT_EQ("A=B; C=D", GetCookiesWithOptions(cm, url_google_, options));

  EXPECT_EQ(2, DeleteAll(cm));
  EXPECT_EQ("", GetCookiesWithOptions(cm, url_google_, options));

  EXPECT_EQ(0u, store->commands().size());

  // Create a persistent cookie.
  EXPECT_TRUE(SetCookie(cm, url_google_,
                        std::string(kValidCookieLine) +
                        "; expires=Mon, 18-Apr-22 22:50:13 GMT"));
  ASSERT_EQ(1u, store->commands().size());
  EXPECT_EQ(CookieStoreCommand::ADD, store->commands()[0].type);

  EXPECT_EQ(1, DeleteAll(cm));  // sync_to_store = true.
  ASSERT_EQ(2u, store->commands().size());
  EXPECT_EQ(CookieStoreCommand::REMOVE, store->commands()[1].type);

  EXPECT_EQ("", GetCookiesWithOptions(cm, url_google_, options));
}

TEST_F(CookieMonsterTest, TestCookieDeleteAllCreatedBetweenTimestamps) {
  scoped_refptr<CookieMonster> cm(new CookieMonster(NULL, NULL));
  Time now = Time::Now();

  // Nothing has been added so nothing should be deleted.
  EXPECT_EQ(0, DeleteAllCreatedBetween(cm, now - TimeDelta::FromDays(99),
                                       Time()));

  // Create 3 cookies with creation date of today, yesterday and the day before.
  EXPECT_TRUE(cm->SetCookieWithCreationTime(url_google_, "T-0=Now", now));
  EXPECT_TRUE(cm->SetCookieWithCreationTime(url_google_, "T-1=Yesterday",
                                            now - TimeDelta::FromDays(1)));
  EXPECT_TRUE(cm->SetCookieWithCreationTime(url_google_, "T-2=DayBefore",
                                            now - TimeDelta::FromDays(2)));
  EXPECT_TRUE(cm->SetCookieWithCreationTime(url_google_, "T-3=ThreeDays",
                                            now - TimeDelta::FromDays(3)));
  EXPECT_TRUE(cm->SetCookieWithCreationTime(url_google_, "T-7=LastWeek",
                                            now - TimeDelta::FromDays(7)));

  // Try to delete threedays and the daybefore.
  EXPECT_EQ(2, DeleteAllCreatedBetween(cm, now - TimeDelta::FromDays(3),
                                       now - TimeDelta::FromDays(1)));

  // Try to delete yesterday, also make sure that delete_end is not
  // inclusive.
  EXPECT_EQ(1, DeleteAllCreatedBetween(cm, now - TimeDelta::FromDays(2),
                                       now));

  // Make sure the delete_begin is inclusive.
  EXPECT_EQ(1, DeleteAllCreatedBetween(cm, now - TimeDelta::FromDays(7),
                                       now));

  // Delete the last (now) item.
  EXPECT_EQ(1, DeleteAllCreatedBetween(cm, Time(), Time()));

  // Really make sure everything is gone.
  EXPECT_EQ(0, DeleteAll(cm));
}

TEST_F(CookieMonsterTest, TestSecure) {
  scoped_refptr<CookieMonster> cm(new CookieMonster(NULL, NULL));

  EXPECT_TRUE(SetCookie(cm, url_google_, "A=B"));
  EXPECT_EQ("A=B", GetCookies(cm, url_google_));
  EXPECT_EQ("A=B", GetCookies(cm, url_google_secure_));

  EXPECT_TRUE(SetCookie(cm, url_google_secure_, "A=B; secure"));
  // The secure should overwrite the non-secure.
  EXPECT_EQ("", GetCookies(cm, url_google_));
  EXPECT_EQ("A=B", GetCookies(cm, url_google_secure_));

  EXPECT_TRUE(SetCookie(cm, url_google_secure_, "D=E; secure"));
  EXPECT_EQ("", GetCookies(cm, url_google_));
  EXPECT_EQ("A=B; D=E", GetCookies(cm, url_google_secure_));

  EXPECT_TRUE(SetCookie(cm, url_google_secure_, "A=B"));
  // The non-secure should overwrite the secure.
  EXPECT_EQ("A=B", GetCookies(cm, url_google_));
  EXPECT_EQ("D=E; A=B", GetCookies(cm, url_google_secure_));
}

static const int kLastAccessThresholdMilliseconds = 200;

TEST_F(CookieMonsterTest, TestLastAccess) {
  scoped_refptr<CookieMonster> cm(
      new CookieMonster(NULL, NULL, kLastAccessThresholdMilliseconds));

  EXPECT_TRUE(SetCookie(cm, url_google_, "A=B"));
  const Time last_access_date(GetFirstCookieAccessDate(cm));

  // Reading the cookie again immediately shouldn't update the access date,
  // since we're inside the threshold.
  EXPECT_EQ("A=B", GetCookies(cm, url_google_));
  EXPECT_TRUE(last_access_date == GetFirstCookieAccessDate(cm));

  // Reading after a short wait should update the access date.
  base::PlatformThread::Sleep(kLastAccessThresholdMilliseconds + 20);
  EXPECT_EQ("A=B", GetCookies(cm, url_google_));
  EXPECT_FALSE(last_access_date == GetFirstCookieAccessDate(cm));
}

TEST_F(CookieMonsterTest, TestHostGarbageCollection) {
  TestHostGarbageCollectHelper(
      CookieMonster::kDomainMaxCookies, CookieMonster::kDomainPurgeCookies,
      CookieMonster::EKS_KEEP_RECENT_AND_PURGE_ETLDP1);
  TestHostGarbageCollectHelper(
      CookieMonster::kDomainMaxCookies, CookieMonster::kDomainPurgeCookies,
      CookieMonster::EKS_DISCARD_RECENT_AND_PURGE_DOMAIN);
}

// Formerly NetUtilTest.CookieTest back when we used wininet's cookie handling.
TEST_F(CookieMonsterTest, NetUtilCookieTest) {
  const GURL test_url("http://mojo.jojo.google.izzle/");

  scoped_refptr<CookieMonster> cm(new CookieMonster(NULL, NULL));

  EXPECT_TRUE(SetCookie(cm, test_url, "foo=bar"));
  std::string value = GetCookies(cm, test_url);
  EXPECT_EQ("foo=bar", value);

  // test that we can retrieve all cookies:
  EXPECT_TRUE(SetCookie(cm, test_url, "x=1"));
  EXPECT_TRUE(SetCookie(cm, test_url, "y=2"));

  std::string result = GetCookies(cm, test_url);
  EXPECT_FALSE(result.empty());
  EXPECT_NE(result.find("x=1"), std::string::npos) << result;
  EXPECT_NE(result.find("y=2"), std::string::npos) << result;
}

TEST_F(CookieMonsterTest, TestDeleteSingleCookie) {
  scoped_refptr<CookieMonster> cm(new CookieMonster(NULL, NULL));

  EXPECT_TRUE(SetCookie(cm, url_google_, "A=B"));
  EXPECT_TRUE(SetCookie(cm, url_google_, "C=D"));
  EXPECT_TRUE(SetCookie(cm, url_google_, "E=F"));
  EXPECT_EQ("A=B; C=D; E=F", GetCookies(cm, url_google_));

  EXPECT_TRUE(FindAndDeleteCookie(cm, url_google_.host(), "C"));
  EXPECT_EQ("A=B; E=F", GetCookies(cm, url_google_));

  EXPECT_FALSE(FindAndDeleteCookie(cm, "random.host", "E"));
  EXPECT_EQ("A=B; E=F", GetCookies(cm, url_google_));
}

TEST_F(CookieMonsterTest, SetCookieableSchemes) {
  scoped_refptr<CookieMonster> cm(new CookieMonster(NULL, NULL));
  scoped_refptr<CookieMonster> cm_foo(new CookieMonster(NULL, NULL));

  // Only cm_foo should allow foo:// cookies.
  const char* kSchemes[] = {"foo"};
  cm_foo->SetCookieableSchemes(kSchemes, 1);

  GURL foo_url("foo://host/path");
  GURL http_url("http://host/path");

  EXPECT_TRUE(SetCookie(cm, http_url, "x=1"));
  EXPECT_FALSE(SetCookie(cm, foo_url, "x=1"));
  EXPECT_TRUE(SetCookie(cm_foo, foo_url, "x=1"));
  EXPECT_FALSE(SetCookie(cm_foo, http_url, "x=1"));
}

TEST_F(CookieMonsterTest, GetAllCookiesForURL) {
  scoped_refptr<CookieMonster> cm(
      new CookieMonster(NULL, NULL, kLastAccessThresholdMilliseconds));

  // Create an httponly cookie.
  CookieOptions options;
  options.set_include_httponly();

  EXPECT_TRUE(SetCookieWithOptions(cm, url_google_, "A=B; httponly", options));
  EXPECT_TRUE(SetCookieWithOptions(cm, url_google_,
                                   "C=D; domain=.google.izzle",
                                   options));
  EXPECT_TRUE(SetCookieWithOptions(cm, url_google_secure_,
                                   "E=F; domain=.google.izzle; secure",
                                   options));

  const Time last_access_date(GetFirstCookieAccessDate(cm));

  base::PlatformThread::Sleep(kLastAccessThresholdMilliseconds + 20);

  // Check cookies for url.
  CookieList cookies = GetAllCookiesForURL(cm, url_google_);
  CookieList::iterator it = cookies.begin();

  ASSERT_TRUE(it != cookies.end());
  EXPECT_EQ("www.google.izzle", it->Domain());
  EXPECT_EQ("A", it->Name());

  ASSERT_TRUE(++it != cookies.end());
  EXPECT_EQ(".google.izzle", it->Domain());
  EXPECT_EQ("C", it->Name());

  ASSERT_TRUE(++it == cookies.end());

  // Check cookies for url excluding http-only cookies.
  cookies =
      GetAllCookiesForURLWithOptions(cm, url_google_, CookieOptions());
  it = cookies.begin();

  ASSERT_TRUE(it != cookies.end());
  EXPECT_EQ(".google.izzle", it->Domain());
  EXPECT_EQ("C", it->Name());

  ASSERT_TRUE(++it == cookies.end());

  // Test secure cookies.
  cookies = GetAllCookiesForURL(cm, url_google_secure_);
  it = cookies.begin();

  ASSERT_TRUE(it != cookies.end());
  EXPECT_EQ("www.google.izzle", it->Domain());
  EXPECT_EQ("A", it->Name());

  ASSERT_TRUE(++it != cookies.end());
  EXPECT_EQ(".google.izzle", it->Domain());
  EXPECT_EQ("C", it->Name());

  ASSERT_TRUE(++it != cookies.end());
  EXPECT_EQ(".google.izzle", it->Domain());
  EXPECT_EQ("E", it->Name());

  ASSERT_TRUE(++it == cookies.end());

  // Reading after a short wait should not update the access date.
  EXPECT_TRUE(last_access_date == GetFirstCookieAccessDate(cm));
}

TEST_F(CookieMonsterTest, GetAllCookiesForURLPathMatching) {
  scoped_refptr<CookieMonster> cm(new CookieMonster(NULL, NULL));
  CookieOptions options;

  EXPECT_TRUE(SetCookieWithOptions(cm, url_google_foo_,
                                   "A=B; path=/foo;", options));
  EXPECT_TRUE(SetCookieWithOptions(cm, url_google_bar_,
                                   "C=D; path=/bar;", options));
  EXPECT_TRUE(SetCookieWithOptions(cm, url_google_,
                                   "E=F;", options));

  CookieList cookies = GetAllCookiesForURL(cm, url_google_foo_);
  CookieList::iterator it = cookies.begin();

  ASSERT_TRUE(it != cookies.end());
  EXPECT_EQ("A", it->Name());
  EXPECT_EQ("/foo", it->Path());

  ASSERT_TRUE(++it != cookies.end());
  EXPECT_EQ("E", it->Name());
  EXPECT_EQ("/", it->Path());

  ASSERT_TRUE(++it == cookies.end());

  cookies = GetAllCookiesForURL(cm, url_google_bar_);
  it = cookies.begin();

  ASSERT_TRUE(it != cookies.end());
  EXPECT_EQ("C", it->Name());
  EXPECT_EQ("/bar", it->Path());

  ASSERT_TRUE(++it != cookies.end());
  EXPECT_EQ("E", it->Name());
  EXPECT_EQ("/", it->Path());

  ASSERT_TRUE(++it == cookies.end());
}

TEST_F(CookieMonsterTest, DeleteCookieByName) {
  scoped_refptr<CookieMonster> cm(new CookieMonster(NULL, NULL));

  EXPECT_TRUE(SetCookie(cm, url_google_, "A=A1; path=/"));
  EXPECT_TRUE(SetCookie(cm, url_google_, "A=A2; path=/foo"));
  EXPECT_TRUE(SetCookie(cm, url_google_, "A=A3; path=/bar"));
  EXPECT_TRUE(SetCookie(cm, url_google_, "B=B1; path=/"));
  EXPECT_TRUE(SetCookie(cm, url_google_, "B=B2; path=/foo"));
  EXPECT_TRUE(SetCookie(cm, url_google_, "B=B3; path=/bar"));

  DeleteCookie(cm, GURL(std::string(kUrlGoogle) + "/foo/bar"), "A");

  CookieList cookies = GetAllCookies(cm);
  size_t expected_size = 4;
  EXPECT_EQ(expected_size, cookies.size());
  for (CookieList::iterator it = cookies.begin();
       it != cookies.end(); ++it) {
    EXPECT_NE("A1", it->Value());
    EXPECT_NE("A2", it->Value());
  }
}

TEST_F(CookieMonsterTest, InitializeFromCookieMonster) {
  scoped_refptr<CookieMonster> cm_1(new CookieMonster(NULL, NULL));
  CookieOptions options;

  EXPECT_TRUE(SetCookieWithOptions(cm_1.get(), url_google_foo_,
                                               "A1=B; path=/foo;",
                                               options));
  EXPECT_TRUE(SetCookieWithOptions(cm_1.get(), url_google_bar_,
                                               "A2=D; path=/bar;",
                                               options));
  EXPECT_TRUE(SetCookieWithOptions(cm_1.get(), url_google_,
                                               "A3=F;",
                                               options));

  CookieList cookies_1 = GetAllCookies(cm_1);
  scoped_refptr<CookieMonster> cm_2(new CookieMonster(NULL, NULL));
  ASSERT_TRUE(cm_2->InitializeFrom(cookies_1));
  CookieList cookies_2 = GetAllCookies(cm_2);

  size_t expected_size = 3;
  EXPECT_EQ(expected_size, cookies_2.size());

  CookieList::iterator it = cookies_2.begin();

  ASSERT_TRUE(it != cookies_2.end());
  EXPECT_EQ("A1", it->Name());
  EXPECT_EQ("/foo", it->Path());

  ASSERT_TRUE(++it != cookies_2.end());
  EXPECT_EQ("A2", it->Name());
  EXPECT_EQ("/bar", it->Path());

  ASSERT_TRUE(++it != cookies_2.end());
  EXPECT_EQ("A3", it->Name());
  EXPECT_EQ("/", it->Path());
}


// Test that overwriting persistent cookies deletes the old one from the
// backing store.
TEST_F(CookieMonsterTest, OverwritePersistentCookie) {
  GURL url_google("http://www.google.com/");
  GURL url_chromium("http://chromium.org");
  scoped_refptr<MockPersistentCookieStore> store(
      new MockPersistentCookieStore);
  scoped_refptr<CookieMonster> cm(new CookieMonster(store, NULL));

  // Insert a cookie "a" for path "/path1"
  EXPECT_TRUE(
      SetCookie(cm, url_google, "a=val1; path=/path1; "
                "expires=Mon, 18-Apr-22 22:50:13 GMT"));
  ASSERT_EQ(1u, store->commands().size());
  EXPECT_EQ(CookieStoreCommand::ADD, store->commands()[0].type);

  // Insert a cookie "b" for path "/path1"
  EXPECT_TRUE(
      SetCookie(cm, url_google, "b=val1; path=/path1; "
                "expires=Mon, 18-Apr-22 22:50:14 GMT"));
  ASSERT_EQ(2u, store->commands().size());
  EXPECT_EQ(CookieStoreCommand::ADD, store->commands()[1].type);

  // Insert a cookie "b" for path "/path1", that is httponly. This should
  // overwrite the non-http-only version.
  CookieOptions allow_httponly;
  allow_httponly.set_include_httponly();
  EXPECT_TRUE(
    SetCookieWithOptions(cm, url_google,
                         "b=val2; path=/path1; httponly; "
                         "expires=Mon, 18-Apr-22 22:50:14 GMT",
                         allow_httponly));
  ASSERT_EQ(4u, store->commands().size());
  EXPECT_EQ(CookieStoreCommand::REMOVE, store->commands()[2].type);
  EXPECT_EQ(CookieStoreCommand::ADD, store->commands()[3].type);

  // Insert a cookie "a" for path "/path1". This should overwrite.
  EXPECT_TRUE(SetCookie(cm, url_google,
                        "a=val33; path=/path1; "
                        "expires=Mon, 18-Apr-22 22:50:14 GMT"));
  ASSERT_EQ(6u, store->commands().size());
  EXPECT_EQ(CookieStoreCommand::REMOVE, store->commands()[4].type);
  EXPECT_EQ(CookieStoreCommand::ADD, store->commands()[5].type);

  // Insert a cookie "a" for path "/path2". This should NOT overwrite
  // cookie "a", since the path is different.
  EXPECT_TRUE(SetCookie(cm, url_google,
                        "a=val9; path=/path2; "
                        "expires=Mon, 18-Apr-22 22:50:14 GMT"));
  ASSERT_EQ(7u, store->commands().size());
  EXPECT_EQ(CookieStoreCommand::ADD, store->commands()[6].type);

  // Insert a cookie "a" for path "/path1", but this time for "chromium.org".
  // Although the name and path match, the hostnames do not, so shouldn't
  // overwrite.
  EXPECT_TRUE(SetCookie(cm, url_chromium,
                        "a=val99; path=/path1; "
                        "expires=Mon, 18-Apr-22 22:50:14 GMT"));
  ASSERT_EQ(8u, store->commands().size());
  EXPECT_EQ(CookieStoreCommand::ADD, store->commands()[7].type);

  EXPECT_EQ("a=val33", GetCookies(cm, GURL("http://www.google.com/path1")));
  EXPECT_EQ("a=val9", GetCookies(cm, GURL("http://www.google.com/path2")));
  EXPECT_EQ("a=val99", GetCookies(cm, GURL("http://chromium.org/path1")));
}

// Tests importing from a persistent cookie store that contains duplicate
// equivalent cookies. This situation should be handled by removing the
// duplicate cookie (both from the in-memory cache, and from the backing store).
//
// This is a regression test for: http://crbug.com/17855.
TEST_F(CookieMonsterTest, DontImportDuplicateCookies) {
  scoped_refptr<MockPersistentCookieStore> store(
      new MockPersistentCookieStore);

  // We will fill some initial cookies into the PersistentCookieStore,
  // to simulate a database with 4 duplicates.  Note that we need to
  // be careful not to have any duplicate creation times at all (as it's a
  // violation of a CookieMonster invariant) even if Time::Now() doesn't
  // move between calls.
  std::vector<CookieMonster::CanonicalCookie*> initial_cookies;

  // Insert 4 cookies with name "X" on path "/", with varying creation
  // dates. We expect only the most recent one to be preserved following
  // the import.

  AddCookieToList("www.google.com",
                  "X=1; path=/; expires=Mon, 18-Apr-22 22:50:14 GMT",
                  Time::Now() + TimeDelta::FromDays(3),
                  &initial_cookies);

  AddCookieToList("www.google.com",
                  "X=2; path=/; expires=Mon, 18-Apr-22 22:50:14 GMT",
                  Time::Now() + TimeDelta::FromDays(1),
                  &initial_cookies);

  // ===> This one is the WINNER (biggest creation time).  <====
  AddCookieToList("www.google.com",
                  "X=3; path=/; expires=Mon, 18-Apr-22 22:50:14 GMT",
                  Time::Now() + TimeDelta::FromDays(4),
                  &initial_cookies);

  AddCookieToList("www.google.com",
                  "X=4; path=/; expires=Mon, 18-Apr-22 22:50:14 GMT",
                  Time::Now(),
                  &initial_cookies);

  // Insert 2 cookies with name "X" on path "/2", with varying creation
  // dates. We expect only the most recent one to be preserved the import.

  // ===> This one is the WINNER (biggest creation time).  <====
  AddCookieToList("www.google.com",
                  "X=a1; path=/2; expires=Mon, 18-Apr-22 22:50:14 GMT",
                  Time::Now() + TimeDelta::FromDays(9),
                  &initial_cookies);

  AddCookieToList("www.google.com",
                  "X=a2; path=/2; expires=Mon, 18-Apr-22 22:50:14 GMT",
                  Time::Now() + TimeDelta::FromDays(2),
                  &initial_cookies);

  // Insert 1 cookie with name "Y" on path "/".
  AddCookieToList("www.google.com",
                  "Y=a; path=/; expires=Mon, 18-Apr-22 22:50:14 GMT",
                  Time::Now() + TimeDelta::FromDays(10),
                  &initial_cookies);

  // Inject our initial cookies into the mock PersistentCookieStore.
  store->SetLoadExpectation(true, initial_cookies);

  scoped_refptr<CookieMonster> cm(new CookieMonster(store, NULL));

  // Verify that duplicates were not imported for path "/".
  // (If this had failed, GetCookies() would have also returned X=1, X=2, X=4).
  EXPECT_EQ("X=3; Y=a", GetCookies(cm, GURL("http://www.google.com/")));

  // Verify that same-named cookie on a different path ("/x2") didn't get
  // messed up.
  EXPECT_EQ("X=a1; X=3; Y=a",
            GetCookies(cm, GURL("http://www.google.com/2/x")));

  // Verify that the PersistentCookieStore was told to kill its 4 duplicates.
  ASSERT_EQ(4u, store->commands().size());
  EXPECT_EQ(CookieStoreCommand::REMOVE, store->commands()[0].type);
  EXPECT_EQ(CookieStoreCommand::REMOVE, store->commands()[1].type);
  EXPECT_EQ(CookieStoreCommand::REMOVE, store->commands()[2].type);
  EXPECT_EQ(CookieStoreCommand::REMOVE, store->commands()[3].type);
}

// Tests importing from a persistent cookie store that contains cookies
// with duplicate creation times.  This situation should be handled by
// dropping the cookies before insertion/visibility to user.
//
// This is a regression test for: http://crbug.com/43188.
TEST_F(CookieMonsterTest, DontImportDuplicateCreationTimes) {
  scoped_refptr<MockPersistentCookieStore> store(
      new MockPersistentCookieStore);

  Time now(Time::Now());
  Time earlier(now - TimeDelta::FromDays(1));

  // Insert 8 cookies, four with the current time as creation times, and
  // four with the earlier time as creation times.  We should only get
  // two cookies remaining, but which two (other than that there should
  // be one from each set) will be random.
  std::vector<CookieMonster::CanonicalCookie*> initial_cookies;
  AddCookieToList("www.google.com", "X=1; path=/", now, &initial_cookies);
  AddCookieToList("www.google.com", "X=2; path=/", now, &initial_cookies);
  AddCookieToList("www.google.com", "X=3; path=/", now, &initial_cookies);
  AddCookieToList("www.google.com", "X=4; path=/", now, &initial_cookies);

  AddCookieToList("www.google.com", "Y=1; path=/", earlier, &initial_cookies);
  AddCookieToList("www.google.com", "Y=2; path=/", earlier, &initial_cookies);
  AddCookieToList("www.google.com", "Y=3; path=/", earlier, &initial_cookies);
  AddCookieToList("www.google.com", "Y=4; path=/", earlier, &initial_cookies);

  // Inject our initial cookies into the mock PersistentCookieStore.
  store->SetLoadExpectation(true, initial_cookies);

  scoped_refptr<CookieMonster> cm(new CookieMonster(store, NULL));

  CookieList list(GetAllCookies(cm));
  EXPECT_EQ(2U, list.size());
  // Confirm that we have one of each.
  std::string name1(list[0].Name());
  std::string name2(list[1].Name());
  EXPECT_TRUE(name1 == "X" || name2 == "X");
  EXPECT_TRUE(name1 == "Y" || name2 == "Y");
  EXPECT_NE(name1, name2);
}

TEST_F(CookieMonsterTest, Delegate) {
  scoped_refptr<MockPersistentCookieStore> store(
      new MockPersistentCookieStore);
  scoped_refptr<MockCookieMonsterDelegate> delegate(
      new MockCookieMonsterDelegate);
  scoped_refptr<CookieMonster> cm(new CookieMonster(store, delegate));

  EXPECT_TRUE(SetCookie(cm, url_google_, "A=B"));
  EXPECT_TRUE(SetCookie(cm, url_google_, "C=D"));
  EXPECT_TRUE(SetCookie(cm, url_google_, "E=F"));
  EXPECT_EQ("A=B; C=D; E=F", GetCookies(cm, url_google_));
  ASSERT_EQ(3u, delegate->changes().size());
  EXPECT_FALSE(delegate->changes()[0].second);
  EXPECT_EQ(url_google_.host(), delegate->changes()[0].first.Domain());
  EXPECT_EQ("A", delegate->changes()[0].first.Name());
  EXPECT_EQ("B", delegate->changes()[0].first.Value());
  EXPECT_EQ(url_google_.host(), delegate->changes()[1].first.Domain());
  EXPECT_FALSE(delegate->changes()[1].second);
  EXPECT_EQ("C", delegate->changes()[1].first.Name());
  EXPECT_EQ("D", delegate->changes()[1].first.Value());
  EXPECT_EQ(url_google_.host(), delegate->changes()[2].first.Domain());
  EXPECT_FALSE(delegate->changes()[2].second);
  EXPECT_EQ("E", delegate->changes()[2].first.Name());
  EXPECT_EQ("F", delegate->changes()[2].first.Value());
  delegate->reset();

  EXPECT_TRUE(FindAndDeleteCookie(cm, url_google_.host(), "C"));
  EXPECT_EQ("A=B; E=F", GetCookies(cm, url_google_));
  ASSERT_EQ(1u, delegate->changes().size());
  EXPECT_EQ(url_google_.host(), delegate->changes()[0].first.Domain());
  EXPECT_TRUE(delegate->changes()[0].second);
  EXPECT_EQ("C", delegate->changes()[0].first.Name());
  EXPECT_EQ("D", delegate->changes()[0].first.Value());
  delegate->reset();

  EXPECT_FALSE(FindAndDeleteCookie(cm, "random.host", "E"));
  EXPECT_EQ("A=B; E=F", GetCookies(cm, url_google_));
  EXPECT_EQ(0u, delegate->changes().size());

  // Insert a cookie "a" for path "/path1"
  EXPECT_TRUE(
      SetCookie(cm, url_google_, "a=val1; path=/path1; "
                                "expires=Mon, 18-Apr-22 22:50:13 GMT"));
  ASSERT_EQ(1u, store->commands().size());
  EXPECT_EQ(CookieStoreCommand::ADD, store->commands()[0].type);
  ASSERT_EQ(1u, delegate->changes().size());
  EXPECT_FALSE(delegate->changes()[0].second);
  EXPECT_EQ(url_google_.host(), delegate->changes()[0].first.Domain());
  EXPECT_EQ("a", delegate->changes()[0].first.Name());
  EXPECT_EQ("val1", delegate->changes()[0].first.Value());
  delegate->reset();

  // Insert a cookie "a" for path "/path1", that is httponly. This should
  // overwrite the non-http-only version.
  CookieOptions allow_httponly;
  allow_httponly.set_include_httponly();
  EXPECT_TRUE(
    SetCookieWithOptions(cm, url_google_,
                         "a=val2; path=/path1; httponly; "
                         "expires=Mon, 18-Apr-22 22:50:14 GMT",
                         allow_httponly));
  ASSERT_EQ(3u, store->commands().size());
  EXPECT_EQ(CookieStoreCommand::REMOVE, store->commands()[1].type);
  EXPECT_EQ(CookieStoreCommand::ADD, store->commands()[2].type);
  ASSERT_EQ(2u, delegate->changes().size());
  EXPECT_EQ(url_google_.host(), delegate->changes()[0].first.Domain());
  EXPECT_TRUE(delegate->changes()[0].second);
  EXPECT_EQ("a", delegate->changes()[0].first.Name());
  EXPECT_EQ("val1", delegate->changes()[0].first.Value());
  EXPECT_EQ(url_google_.host(), delegate->changes()[1].first.Domain());
  EXPECT_FALSE(delegate->changes()[1].second);
  EXPECT_EQ("a", delegate->changes()[1].first.Name());
  EXPECT_EQ("val2", delegate->changes()[1].first.Value());
  delegate->reset();
}

TEST_F(CookieMonsterTest, SetCookieWithDetails) {
  scoped_refptr<CookieMonster> cm(new CookieMonster(NULL, NULL));

  EXPECT_TRUE(SetCookieWithDetails(
      cm, url_google_foo_, "A", "B", std::string(), "/foo", base::Time(),
      false, false));
  EXPECT_TRUE(SetCookieWithDetails(
      cm, url_google_bar_, "C", "D", "google.izzle", "/bar", base::Time(),
      false, true));
  EXPECT_TRUE(SetCookieWithDetails(
      cm, url_google_, "E", "F", std::string(), std::string(), base::Time(),
      true, false));

  // Test that malformed attributes fail to set the cookie.
  EXPECT_FALSE(SetCookieWithDetails(
      cm, url_google_foo_, " A", "B", std::string(), "/foo", base::Time(),
      false, false));
  EXPECT_FALSE(SetCookieWithDetails(
      cm, url_google_foo_, "A;", "B", std::string(), "/foo", base::Time(),
      false, false));
  EXPECT_FALSE(SetCookieWithDetails(
      cm, url_google_foo_, "A=", "B", std::string(), "/foo", base::Time(),
      false, false));
  EXPECT_FALSE(SetCookieWithDetails(
      cm, url_google_foo_, "A", "B", "google.ozzzzzzle", "foo", base::Time(),
      false, false));
  EXPECT_FALSE(SetCookieWithDetails(
      cm, url_google_foo_, "A=", "B", std::string(), "foo", base::Time(),
      false, false));

  CookieList cookies = GetAllCookiesForURL(cm, url_google_foo_);
  CookieList::iterator it = cookies.begin();

  ASSERT_TRUE(it != cookies.end());
  EXPECT_EQ("A", it->Name());
  EXPECT_EQ("B", it->Value());
  EXPECT_EQ("www.google.izzle", it->Domain());
  EXPECT_EQ("/foo", it->Path());
  EXPECT_FALSE(it->DoesExpire());
  EXPECT_FALSE(it->IsSecure());
  EXPECT_FALSE(it->IsHttpOnly());

  ASSERT_TRUE(++it == cookies.end());

  cookies = GetAllCookiesForURL(cm, url_google_bar_);
  it = cookies.begin();

  ASSERT_TRUE(it != cookies.end());
  EXPECT_EQ("C", it->Name());
  EXPECT_EQ("D", it->Value());
  EXPECT_EQ(".google.izzle", it->Domain());
  EXPECT_EQ("/bar", it->Path());
  EXPECT_FALSE(it->IsSecure());
  EXPECT_TRUE(it->IsHttpOnly());

  ASSERT_TRUE(++it == cookies.end());

  cookies = GetAllCookiesForURL(cm, url_google_secure_);
  it = cookies.begin();

  ASSERT_TRUE(it != cookies.end());
  EXPECT_EQ("E", it->Name());
  EXPECT_EQ("F", it->Value());
  EXPECT_EQ("/", it->Path());
  EXPECT_EQ("www.google.izzle", it->Domain());
  EXPECT_TRUE(it->IsSecure());
  EXPECT_FALSE(it->IsHttpOnly());

  ASSERT_TRUE(++it == cookies.end());
}

TEST_F(CookieMonsterTest, DeleteAllForHost) {
  scoped_refptr<CookieMonster> cm(new CookieMonster(NULL, NULL));

  // Test probes:
  //    * Non-secure URL, mid-level (http://w.c.b.a)
  //    * Secure URL, mid-level (https://w.c.b.a)
  //    * URL with path, mid-level (https:/w.c.b.a/dir1/xx)
  // All three tests should nuke only the midlevel host cookie,
  // the http_only cookie, the host secure cookie, and the two host
  // path cookies.  http_only, secure, and paths are ignored by
  // this call, and domain cookies arent touched.
  PopulateCmForDeleteAllForHost(cm);
  EXPECT_EQ("dom_1=X; dom_2=X; dom_3=X; host_3=X",
            GetCookies(cm, GURL(kTopLevelDomainPlus3)));
  EXPECT_EQ("dom_1=X; dom_2=X; host_2=X; sec_dom=X; sec_host=X",
            GetCookies(cm, GURL(kTopLevelDomainPlus2Secure)));
  EXPECT_EQ("dom_1=X; host_1=X", GetCookies(cm, GURL(kTopLevelDomainPlus1)));
  EXPECT_EQ("dom_path_2=X; host_path_2=X; dom_path_1=X; host_path_1=X; "
            "dom_1=X; dom_2=X; host_2=X; sec_dom=X; sec_host=X",
            GetCookies(cm, GURL(kTopLevelDomainPlus2Secure +
                                std::string("/dir1/dir2/xxx"))));

  EXPECT_EQ(5, DeleteAllForHost(cm, GURL(kTopLevelDomainPlus2)));
  EXPECT_EQ(8U, GetAllCookies(cm).size());

  EXPECT_EQ("dom_1=X; dom_2=X; dom_3=X; host_3=X",
            GetCookies(cm, GURL(kTopLevelDomainPlus3)));
  EXPECT_EQ("dom_1=X; dom_2=X; sec_dom=X",
            GetCookies(cm, GURL(kTopLevelDomainPlus2Secure)));
  EXPECT_EQ("dom_1=X; host_1=X", GetCookies(cm, GURL(kTopLevelDomainPlus1)));
  EXPECT_EQ("dom_path_2=X; dom_path_1=X; dom_1=X; dom_2=X; sec_dom=X",
            GetCookies(cm, GURL(kTopLevelDomainPlus2Secure +
                                std::string("/dir1/dir2/xxx"))));

  PopulateCmForDeleteAllForHost(cm);
  EXPECT_EQ(5, DeleteAllForHost(cm, GURL(kTopLevelDomainPlus2Secure)));
  EXPECT_EQ(8U, GetAllCookies(cm).size());

  EXPECT_EQ("dom_1=X; dom_2=X; dom_3=X; host_3=X",
            GetCookies(cm, GURL(kTopLevelDomainPlus3)));
  EXPECT_EQ("dom_1=X; dom_2=X; sec_dom=X",
            GetCookies(cm, GURL(kTopLevelDomainPlus2Secure)));
  EXPECT_EQ("dom_1=X; host_1=X", GetCookies(cm, GURL(kTopLevelDomainPlus1)));
  EXPECT_EQ("dom_path_2=X; dom_path_1=X; dom_1=X; dom_2=X; sec_dom=X",
            GetCookies(cm, GURL(kTopLevelDomainPlus2Secure +
                                std::string("/dir1/dir2/xxx"))));

  PopulateCmForDeleteAllForHost(cm);
  EXPECT_EQ(5, DeleteAllForHost(cm, GURL(kTopLevelDomainPlus2Secure +
                                         std::string("/dir1/xxx"))));
  EXPECT_EQ(8U, GetAllCookies(cm).size());

  EXPECT_EQ("dom_1=X; dom_2=X; dom_3=X; host_3=X",
            GetCookies(cm, GURL(kTopLevelDomainPlus3)));
  EXPECT_EQ("dom_1=X; dom_2=X; sec_dom=X",
            GetCookies(cm, GURL(kTopLevelDomainPlus2Secure)));
  EXPECT_EQ("dom_1=X; host_1=X", GetCookies(cm, GURL(kTopLevelDomainPlus1)));
  EXPECT_EQ("dom_path_2=X; dom_path_1=X; dom_1=X; dom_2=X; sec_dom=X",
            GetCookies(cm, GURL(kTopLevelDomainPlus2Secure +
                                std::string("/dir1/dir2/xxx"))));
}

TEST_F(CookieMonsterTest, UniqueCreationTime) {
  scoped_refptr<CookieMonster> cm(new CookieMonster(NULL, NULL));
  CookieOptions options;

  // Add in three cookies through every public interface to the
  // CookieMonster and confirm that none of them have duplicate
  // creation times.

  // SetCookieWithCreationTime and SetCookieWithCreationTimeAndOptions
  // are not included as they aren't going to be public for very much
  // longer.

  // SetCookie, SetCookieWithOptions, SetCookieWithDetails

  SetCookie(cm, url_google_, "SetCookie1=A");
  SetCookie(cm, url_google_, "SetCookie2=A");
  SetCookie(cm, url_google_, "SetCookie3=A");

  SetCookieWithOptions(cm, url_google_, "setCookieWithOptions1=A", options);
  SetCookieWithOptions(cm, url_google_, "setCookieWithOptions2=A", options);
  SetCookieWithOptions(cm, url_google_, "setCookieWithOptions3=A", options);

  SetCookieWithDetails(cm, url_google_, "setCookieWithDetails1", "A",
                       ".google.com", "/", Time(), false, false);
  SetCookieWithDetails(cm, url_google_, "setCookieWithDetails2", "A",
                       ".google.com", "/", Time(), false, false);
  SetCookieWithDetails(cm, url_google_, "setCookieWithDetails3", "A",
                       ".google.com", "/", Time(), false, false);

  // Now we check
  CookieList cookie_list(GetAllCookies(cm));
  typedef std::map<int64, CookieMonster::CanonicalCookie> TimeCookieMap;
  TimeCookieMap check_map;
  for (CookieList::const_iterator it = cookie_list.begin();
       it != cookie_list.end(); it++) {
    const int64 creation_date = it->CreationDate().ToInternalValue();
    TimeCookieMap::const_iterator
        existing_cookie_it(check_map.find(creation_date));
    EXPECT_TRUE(existing_cookie_it == check_map.end())
        << "Cookie " << it->Name() << " has same creation date ("
        << it->CreationDate().ToInternalValue()
        << ") as previously entered cookie "
        << existing_cookie_it->second.Name();

    if (existing_cookie_it == check_map.end()) {
      check_map.insert(TimeCookieMap::value_type(
          it->CreationDate().ToInternalValue(), *it));
    }
  }
}

// Mainly a test of GetEffectiveDomain, or more specifically, of the
// expected behavior of GetEffectiveDomain within the CookieMonster.
TEST_F(CookieMonsterTest, GetKey) {
  scoped_refptr<CookieMonster> cm(new CookieMonster(NULL, NULL));

  // This test is really only interesting if GetKey() actually does something.
  if (cm->expiry_and_key_scheme_ ==
      CookieMonster::EKS_KEEP_RECENT_AND_PURGE_ETLDP1) {
    EXPECT_EQ("google.com", cm->GetKey("www.google.com"));
    EXPECT_EQ("google.izzie", cm->GetKey("www.google.izzie"));
    EXPECT_EQ("google.izzie", cm->GetKey(".google.izzie"));
    EXPECT_EQ("bbc.co.uk", cm->GetKey("bbc.co.uk"));
    EXPECT_EQ("bbc.co.uk", cm->GetKey("a.b.c.d.bbc.co.uk"));
    EXPECT_EQ("apple.com", cm->GetKey("a.b.c.d.apple.com"));
    EXPECT_EQ("apple.izzie", cm->GetKey("a.b.c.d.apple.izzie"));

    // Cases where the effective domain is null, so we use the host
    // as the key.
    EXPECT_EQ("co.uk", cm->GetKey("co.uk"));
    const std::string extension_name("iehocdgbbocmkdidlbnnfbmbinnahbae");
    EXPECT_EQ(extension_name, cm->GetKey(extension_name));
    EXPECT_EQ("com", cm->GetKey("com"));
    EXPECT_EQ("hostalias", cm->GetKey("hostalias"));
    EXPECT_EQ("localhost", cm->GetKey("localhost"));
  }
}

// Test that cookies transfer from/to the backing store correctly.
TEST_F(CookieMonsterTest, BackingStoreCommunication) {
  // Store details for cookies transforming through the backing store interface.

  base::Time current(base::Time::Now());
  scoped_refptr<MockSimplePersistentCookieStore> store(
      new MockSimplePersistentCookieStore);
  base::Time new_access_time;
  base::Time expires(base::Time::Now() + base::TimeDelta::FromSeconds(100));

  struct CookiesInputInfo {
    std::string gurl;
    std::string name;
    std::string value;
    std::string domain;
    std::string path;
    base::Time expires;
    bool secure;
    bool http_only;
  };
  const CookiesInputInfo input_info[] = {
    {"http://a.b.google.com", "a", "1", "", "/path/to/cookie", expires,
     false, false},
    {"https://www.google.com", "b", "2", ".google.com", "/path/from/cookie",
     expires + TimeDelta::FromSeconds(10), true, true},
    {"https://google.com", "c", "3", "", "/another/path/to/cookie",
     base::Time::Now() + base::TimeDelta::FromSeconds(100),
     true, false}
  };
  const int INPUT_DELETE = 1;

  // Create new cookies and flush them to the store.
  {
    scoped_refptr<CookieMonster> cmout(new CookieMonster(store, NULL));
    for (const CookiesInputInfo* p = input_info;
         p < &input_info[ARRAYSIZE_UNSAFE(input_info)]; p++) {
      EXPECT_TRUE(SetCookieWithDetails(cmout, GURL(p->gurl), p->name, p->value,
                                       p->domain, p->path, p->expires,
                                       p->secure, p->http_only));
    }
    DeleteCookie(cmout, GURL(std::string(input_info[INPUT_DELETE].gurl) +
                             input_info[INPUT_DELETE].path),
                 input_info[INPUT_DELETE].name);
  }

  // Create a new cookie monster and make sure that everything is correct
  {
    scoped_refptr<CookieMonster> cmin(new CookieMonster(store, NULL));
    CookieList cookies(GetAllCookies(cmin));
    ASSERT_EQ(2u, cookies.size());
    // Ordering is path length, then creation time.  So second cookie
    // will come first, and we need to swap them.
    std::swap(cookies[0], cookies[1]);
    for (int output_index = 0; output_index < 2; output_index++) {
      int input_index = output_index * 2;
      const CookiesInputInfo* input = &input_info[input_index];
      const CookieMonster::CanonicalCookie* output = &cookies[output_index];

      EXPECT_EQ(input->name, output->Name());
      EXPECT_EQ(input->value, output->Value());
      EXPECT_EQ(GURL(input->gurl).host(), output->Domain());
      EXPECT_EQ(input->path, output->Path());
      EXPECT_LE(current.ToInternalValue(),
                output->CreationDate().ToInternalValue());
      EXPECT_EQ(input->secure, output->IsSecure());
      EXPECT_EQ(input->http_only, output->IsHttpOnly());
      EXPECT_TRUE(output->IsPersistent());
      EXPECT_EQ(input->expires.ToInternalValue(),
                output->ExpiryDate().ToInternalValue());
    }
  }
}

TEST_F(CookieMonsterTest, CookieOrdering) {
  // Put a random set of cookies into a monster and make sure
  // they're returned in the right order.
  scoped_refptr<CookieMonster> cm(new CookieMonster(NULL, NULL));
  EXPECT_TRUE(SetCookie(cm, GURL("http://d.c.b.a.google.com/aa/x.html"),
                        "c=1"));
  EXPECT_TRUE(SetCookie(cm, GURL("http://b.a.google.com/aa/bb/cc/x.html"),
                        "d=1; domain=b.a.google.com"));
  EXPECT_TRUE(SetCookie(cm, GURL("http://b.a.google.com/aa/bb/cc/x.html"),
                        "a=4; domain=b.a.google.com"));
  EXPECT_TRUE(SetCookie(cm, GURL("http://c.b.a.google.com/aa/bb/cc/x.html"),
                        "e=1; domain=c.b.a.google.com"));
  EXPECT_TRUE(SetCookie(cm, GURL("http://d.c.b.a.google.com/aa/bb/x.html"),
                        "b=1"));
  EXPECT_TRUE(SetCookie(cm, GURL("http://news.bbc.co.uk/midpath/x.html"),
                        "g=10"));
  EXPECT_EQ("d=1; a=4; e=1; b=1; c=1",
            GetCookiesWithOptions(
                cm, GURL("http://d.c.b.a.google.com/aa/bb/cc/dd"),
                CookieOptions()));
  {
    unsigned int i = 0;
    CookieList cookies(GetAllCookiesForURL(
        cm, GURL("http://d.c.b.a.google.com/aa/bb/cc/dd")));
    ASSERT_EQ(5u, cookies.size());
    EXPECT_EQ("d", cookies[i++].Name());
    EXPECT_EQ("a", cookies[i++].Name());
    EXPECT_EQ("e", cookies[i++].Name());
    EXPECT_EQ("b", cookies[i++].Name());
    EXPECT_EQ("c", cookies[i++].Name());
  }

  {
    unsigned int i = 0;
    CookieList cookies(GetAllCookies(cm));
    ASSERT_EQ(6u, cookies.size());
    EXPECT_EQ("d", cookies[i++].Name());
    EXPECT_EQ("a", cookies[i++].Name());
    EXPECT_EQ("e", cookies[i++].Name());
    EXPECT_EQ("g", cookies[i++].Name());
    EXPECT_EQ("b", cookies[i++].Name());
    EXPECT_EQ("c", cookies[i++].Name());
  }
}

// This test and CookieMonstertest.TestGCTimes (in cookie_monster_perftest.cc)
// are somewhat complementary twins.  This test is probing for whether
// garbage collection always happens when it should (i.e. that we actually
// get rid of cookies when we should).  The perftest is probing for
// whether garbage collection happens when it shouldn't.  See comments
// before that test for more details.
TEST_F(CookieMonsterTest, GarbageCollectionTriggers) {
  // First we check to make sure that a whole lot of recent cookies
  // doesn't get rid of anything after garbage collection is checked for.
  {
    scoped_refptr<CookieMonster> cm(
        CreateMonsterForGC(CookieMonster::kMaxCookies * 2));
    EXPECT_EQ(CookieMonster::kMaxCookies * 2, GetAllCookies(cm).size());
    SetCookie(cm, GURL("http://newdomain.com"), "b=2");
    EXPECT_EQ(CookieMonster::kMaxCookies * 2 + 1, GetAllCookies(cm).size());
  }

  // Now we explore a series of relationships between cookie last access
  // time and size of store to make sure we only get rid of cookies when
  // we really should.
  const struct TestCase {
    int num_cookies;
    int num_old_cookies;
    int expected_initial_cookies;
    // Indexed by ExpiryAndKeyScheme
    int expected_cookies_after_set[CookieMonster::EKS_LAST_ENTRY];
  } test_cases[] = {
    {
      // A whole lot of recent cookies; gc shouldn't happen.
      CookieMonster::kMaxCookies * 2,
      0,
      CookieMonster::kMaxCookies * 2,
      { CookieMonster::kMaxCookies * 2 + 1,
        CookieMonster::kMaxCookies - CookieMonster::kPurgeCookies }
    }, {
      // Some old cookies, but still overflowing max.
      CookieMonster::kMaxCookies * 2,
      CookieMonster::kMaxCookies / 2,
      CookieMonster::kMaxCookies * 2,
      {CookieMonster::kMaxCookies * 2 - CookieMonster::kMaxCookies / 2 + 1,
       CookieMonster::kMaxCookies - CookieMonster::kPurgeCookies}
    }, {
      // Old cookies enough to bring us right down to our purge line.
      CookieMonster::kMaxCookies * 2,
      CookieMonster::kMaxCookies + CookieMonster::kPurgeCookies + 1,
      CookieMonster::kMaxCookies * 2,
      {CookieMonster::kMaxCookies - CookieMonster::kPurgeCookies,
       CookieMonster::kMaxCookies - CookieMonster::kPurgeCookies}
    }, {
      // Old cookies enough to bring below our purge line (which we
      // shouldn't do).
      CookieMonster::kMaxCookies * 2,
      CookieMonster::kMaxCookies * 3 / 2,
      CookieMonster::kMaxCookies * 2,
      {CookieMonster::kMaxCookies - CookieMonster::kPurgeCookies,
       CookieMonster::kMaxCookies - CookieMonster::kPurgeCookies}
    }
  };
  const CookieMonster::ExpiryAndKeyScheme schemes[] = {
    CookieMonster::EKS_KEEP_RECENT_AND_PURGE_ETLDP1,
    CookieMonster::EKS_DISCARD_RECENT_AND_PURGE_DOMAIN,
  };

  for (int ci = 0; ci < static_cast<int>(ARRAYSIZE_UNSAFE(test_cases)); ++ci) {
    // Test both old and new key and expiry schemes.
    for (int recent_scheme = 0;
         recent_scheme < static_cast<int>(ARRAYSIZE_UNSAFE(schemes));
         recent_scheme++) {
      const TestCase *test_case = &test_cases[ci];
      scoped_refptr<CookieMonster> cm(
          CreateMonsterFromStoreForGC(
              test_case->num_cookies, test_case->num_old_cookies,
              CookieMonster::kSafeFromGlobalPurgeDays * 2));
      cm->SetExpiryAndKeyScheme(schemes[recent_scheme]);
      EXPECT_EQ(test_case->expected_initial_cookies,
                static_cast<int>(GetAllCookies(cm).size()))
          << "For test case " << ci;
      // Will trigger GC
      SetCookie(cm, GURL("http://newdomain.com"), "b=2");
      EXPECT_EQ(test_case->expected_cookies_after_set[recent_scheme],
                static_cast<int>((GetAllCookies(cm).size())))
          << "For test case (" << ci << ", " << recent_scheme << ")";
    }
  }
}

// This test checks that setting a cookie forcing it to be a session only
// cookie works as expected.
TEST_F(CookieMonsterTest, ForceSessionOnly) {
  scoped_refptr<CookieMonster> cm(new CookieMonster(NULL, NULL));
  CookieOptions options;

  // Set a persistent cookie, but force it to be a session cookie.
  options.set_force_session();
  ASSERT_TRUE(SetCookieWithOptions(
      cm, url_google_,
      std::string(kValidCookieLine) + "; expires=Mon, 18-Apr-22 22:50:13 GMT",
      options));

  // Get the canonical cookie.
  CookieList cookie_list = GetAllCookies(cm);
  ASSERT_EQ(1U, cookie_list.size());
  ASSERT_FALSE(cookie_list[0].IsPersistent());

  // Use a past expiry date to delete the cookie, but force it to session only.
  ASSERT_TRUE(SetCookieWithOptions(
      cm, url_google_,
      std::string(kValidCookieLine) + "; expires=Mon, 18-Apr-1977 22:50:13 GMT",
      options));

  // Check that the cookie was deleted.
  cookie_list = GetAllCookies(cm);
  ASSERT_EQ(0U, cookie_list.size());
}

// This test checks that keep expired cookies flag is working.
TEST_F(CookieMonsterTest, KeepExpiredCookies) {
  scoped_refptr<CookieMonster> cm(new CookieMonster(NULL, NULL));
  cm->SetKeepExpiredCookies();
  CookieOptions options;

  // Set a persistent cookie.
  ASSERT_TRUE(SetCookieWithOptions(
      cm, url_google_,
      std::string(kValidCookieLine) + "; expires=Mon, 18-Apr-22 22:50:13 GMT",
      options));

  // Get the canonical cookie.
  CookieList cookie_list = GetAllCookies(cm);
  ASSERT_EQ(1U, cookie_list.size());

  // Use a past expiry date to delete the cookie.
  ASSERT_TRUE(SetCookieWithOptions(
      cm, url_google_,
      std::string(kValidCookieLine) + "; expires=Mon, 18-Apr-1977 22:50:13 GMT",
      options));

  // Check that the cookie with the past expiry date is still there.
  // GetAllCookies() also triggers garbage collection.
  cookie_list = GetAllCookies(cm);
  ASSERT_EQ(1U, cookie_list.size());
  ASSERT_TRUE(cookie_list[0].IsExpired(Time::Now()));
}

namespace {

// Mock PersistentCookieStore that keeps track of the number of Flush() calls.
class FlushablePersistentStore : public CookieMonster::PersistentCookieStore {
 public:
  FlushablePersistentStore() : flush_count_(0) {}

  void Load(const LoadedCallback& loaded_callback) {
    std::vector<CookieMonster::CanonicalCookie*> out_cookies;
    MessageLoop::current()->PostTask(FROM_HERE,
      base::Bind(&net::LoadedCallbackTask::Run,
                 new net::LoadedCallbackTask(loaded_callback, out_cookies)));
  }

  void LoadCookiesForKey(const std::string& key,
      const LoadedCallback& loaded_callback) {
    Load(loaded_callback);
  }

  void AddCookie(const CookieMonster::CanonicalCookie&) {}
  void UpdateCookieAccessTime(const CookieMonster::CanonicalCookie&) {}
  void DeleteCookie(const CookieMonster::CanonicalCookie&) {}
  void SetClearLocalStateOnExit(bool clear_local_state) {}

  void Flush(Task* completion_callback) {
    ++flush_count_;
    if (completion_callback) {
      completion_callback->Run();
      delete completion_callback;
    }
  }

  int flush_count() {
    return flush_count_;
  }

 private:
  volatile int flush_count_;
};

// Counts the number of times Callback() has been run.
class CallbackCounter : public base::RefCountedThreadSafe<CallbackCounter> {
 public:
  CallbackCounter() : callback_count_(0) {}

  void Callback() {
    ++callback_count_;
  }

  int callback_count() {
    return callback_count_;
  }

 private:
  friend class base::RefCountedThreadSafe<CallbackCounter>;
  volatile int callback_count_;
};

}  // namespace

// Test that FlushStore() is forwarded to the store and callbacks are posted.
TEST_F(CookieMonsterTest, FlushStore) {
  scoped_refptr<CallbackCounter> counter(new CallbackCounter());
  scoped_refptr<FlushablePersistentStore> store(new FlushablePersistentStore());
  scoped_refptr<CookieMonster> cm(new CookieMonster(store, NULL));

  ASSERT_EQ(0, store->flush_count());
  ASSERT_EQ(0, counter->callback_count());

  // Before initialization, FlushStore() should just run the callback.
  cm->FlushStore(NewRunnableMethod(counter.get(), &CallbackCounter::Callback));
  MessageLoop::current()->RunAllPending();

  ASSERT_EQ(0, store->flush_count());
  ASSERT_EQ(1, counter->callback_count());

  // NULL callback is safe.
  cm->FlushStore(NULL);
  MessageLoop::current()->RunAllPending();

  ASSERT_EQ(0, store->flush_count());
  ASSERT_EQ(1, counter->callback_count());

  // After initialization, FlushStore() should delegate to the store.
  GetAllCookies(cm);  // Force init.
  cm->FlushStore(NewRunnableMethod(counter.get(), &CallbackCounter::Callback));
  MessageLoop::current()->RunAllPending();

  ASSERT_EQ(1, store->flush_count());
  ASSERT_EQ(2, counter->callback_count());

  // NULL callback is still safe.
  cm->FlushStore(NULL);
  MessageLoop::current()->RunAllPending();

  ASSERT_EQ(2, store->flush_count());
  ASSERT_EQ(2, counter->callback_count());

  // If there's no backing store, FlushStore() is always a safe no-op.
  cm = new CookieMonster(NULL, NULL);
  GetAllCookies(cm);  // Force init.
  cm->FlushStore(NULL);
  MessageLoop::current()->RunAllPending();

  ASSERT_EQ(2, counter->callback_count());

  cm->FlushStore(NewRunnableMethod(counter.get(), &CallbackCounter::Callback));
  MessageLoop::current()->RunAllPending();

  ASSERT_EQ(3, counter->callback_count());
}

TEST_F(CookieMonsterTest, GetCookieSourceFromURL) {
  EXPECT_EQ("http://example.com/",
            CookieMonster::CanonicalCookie::GetCookieSourceFromURL(
                GURL("http://example.com")));
  EXPECT_EQ("http://example.com/",
            CookieMonster::CanonicalCookie::GetCookieSourceFromURL(
                GURL("http://example.com/")));
  EXPECT_EQ("http://example.com/",
            CookieMonster::CanonicalCookie::GetCookieSourceFromURL(
                GURL("http://example.com/test")));
  EXPECT_EQ("file:///tmp/test.html",
            CookieMonster::CanonicalCookie::GetCookieSourceFromURL(
                GURL("file:///tmp/test.html")));
  EXPECT_EQ("http://example.com/",
            CookieMonster::CanonicalCookie::GetCookieSourceFromURL(
                GURL("http://example.com:1234/")));
  EXPECT_EQ("http://example.com/",
            CookieMonster::CanonicalCookie::GetCookieSourceFromURL(
                GURL("https://example.com/")));
  EXPECT_EQ("http://example.com/",
            CookieMonster::CanonicalCookie::GetCookieSourceFromURL(
                GURL("http://user:pwd@example.com/")));
  EXPECT_EQ("http://example.com/",
            CookieMonster::CanonicalCookie::GetCookieSourceFromURL(
                GURL("http://example.com/test?foo")));
  EXPECT_EQ("http://example.com/",
            CookieMonster::CanonicalCookie::GetCookieSourceFromURL(
                GURL("http://example.com/test#foo")));
}

TEST_F(CookieMonsterTest, HistogramCheck) {
  scoped_refptr<CookieMonster> cm(new CookieMonster(NULL, NULL));
  // Should match call in InitializeHistograms, but doesn't really matter
  // since the histogram should have been initialized by the CM construction
  // above.
  base::Histogram* expired_histogram =
      base::Histogram::FactoryGet(
          "Cookie.ExpirationDurationMinutes", 1, 10 * 365 * 24 * 60, 50,
          base::Histogram::kUmaTargetedHistogramFlag);

  base::Histogram::SampleSet histogram_set_1;
  expired_histogram->SnapshotSample(&histogram_set_1);
  ASSERT_TRUE(SetCookieWithDetails(
      cm, GURL("http://fake.a.url"), "a", "b", "a.url", "/",
      base::Time::Now() + base::TimeDelta::FromMinutes(59),
      false, false));

  base::Histogram::SampleSet histogram_set_2;
  expired_histogram->SnapshotSample(&histogram_set_2);
  EXPECT_EQ(histogram_set_1.TotalCount() + 1,
            histogram_set_2.TotalCount());

  // kValidCookieLine creates a session cookie.
  ASSERT_TRUE(SetCookie(cm, url_google_, kValidCookieLine));
  expired_histogram->SnapshotSample(&histogram_set_1);
  EXPECT_EQ(histogram_set_2.TotalCount(),
            histogram_set_1.TotalCount());
}

namespace {

class MultiThreadedCookieMonsterTest : public CookieMonsterTest {
 public:
  MultiThreadedCookieMonsterTest() : other_thread_("CMTthread") {}

  // Helper methods for calling the asynchronous CookieMonster methods
  // from a different thread.

  void GetCookiesTask(CookieMonster* cm,
                      const GURL& url,
                      GetCookieStringCallback* callback) {
    cm->GetCookiesWithOptionsAsync(
        url, CookieOptions(),
        base::Bind(&GetCookieStringCallback::Run, base::Unretained(callback)));
  }

  void GetCookiesWithOptionsTask(CookieMonster* cm,
                                 const GURL& url,
                                 const CookieOptions& options,
                                 GetCookieStringCallback* callback) {
    cm->GetCookiesWithOptionsAsync(
        url, options,
        base::Bind(&GetCookieStringCallback::Run, base::Unretained(callback)));
  }

  void GetCookiesWithInfoTask(CookieMonster* cm,
                              const GURL& url,
                              const CookieOptions& options,
                              GetCookiesWithInfoCallback* callback) {
    cm->GetCookiesWithInfoAsync(
        url, options,
        base::Bind(&GetCookiesWithInfoCallback::Run,
                   base::Unretained(callback)));
  }

  void GetAllCookiesTask(CookieMonster* cm,
                         GetCookieListCallback* callback) {
    cm->GetAllCookiesAsync(
        base::Bind(&GetCookieListCallback::Run, base::Unretained(callback)));
  }

  void GetAllCookiesForURLTask(CookieMonster* cm,
                               const GURL& url,
                               GetCookieListCallback* callback) {
    cm->GetAllCookiesForURLAsync(
        url,
        base::Bind(&GetCookieListCallback::Run, base::Unretained(callback)));
  }

  void GetAllCookiesForURLWithOptionsTask(CookieMonster* cm,
                                          const GURL& url,
                                          const CookieOptions& options,
                                          GetCookieListCallback* callback) {
    cm->GetAllCookiesForURLWithOptionsAsync(
        url, options,
        base::Bind(&GetCookieListCallback::Run, base::Unretained(callback)));
  }

  void SetCookieWithOptionsTask(CookieMonster* cm,
                                const GURL& url,
                                const std::string& cookie_line,
                                const CookieOptions& options,
                                SetCookieCallback* callback) {
    cm->SetCookieWithOptionsAsync(
        url, cookie_line, options,
        base::Bind(&SetCookieCallback::Run, base::Unretained(callback)));
  }

  void SetCookieWithDetailsTask(CookieMonster* cm, const GURL& url,
                                SetCookieCallback* callback) {
    // Define the parameters here instead of in the calling fucntion.
    // The maximum number of parameters for Bind function is 6.
    std::string name = "A";
    std::string value = "B";
    std::string domain = std::string();
    std::string path = "/foo";
    base::Time expiration_time = base::Time();
    bool secure = false;
    bool http_only = false;
    cm->SetCookieWithDetailsAsync(
        url, name, value, domain, path, expiration_time, secure, http_only,
        base::Bind(&SetCookieCallback::Run, base::Unretained(callback)));
  }

  void DeleteCookieTask(CookieMonster* cm,
                        const GURL& url,
                        const std::string& cookie_name,
                        DeleteCookieCallback* callback) {
    cm->DeleteCookieAsync(
        url, cookie_name,
        base::Bind(&DeleteCookieCallback::Run, base::Unretained(callback)));
  }

  void DeleteAllCreatedBetweenTask(CookieMonster* cm,
                                   const base::Time& delete_begin,
                                   const base::Time& delete_end,
                                   DeleteCallback* callback) {
    cm->DeleteAllCreatedBetweenAsync(
        delete_begin, delete_end,
        base::Bind(&DeleteCallback::Run,
                   base::Unretained(callback)));
  }

  void DeleteAllForHostTask(CookieMonster* cm,
                            const GURL& url,
                            DeleteCallback* callback) {
    cm->DeleteAllForHostAsync(
        url,
        base::Bind(&DeleteCallback::Run, base::Unretained(callback)));
  }

  void DeleteCanonicalCookieTask(CookieMonster* cm,
                                 const CookieMonster::CanonicalCookie& cookie,
                                 SetCookieCallback* callback) {
    cm->DeleteCanonicalCookieAsync(
        cookie,
        base::Bind(&SetCookieCallback::Run, base::Unretained(callback)));
  }

 protected:
  void RunOnOtherThread(const base::Closure& task) {
    other_thread_.Start();
    other_thread_.message_loop()->PostTask(FROM_HERE, task);
    RunFor(kTimeout);
    other_thread_.Stop();
  }

  Thread other_thread_;
};

}  // namespace

// TODO(ycxiao): Eventually, we will need to create a separate thread, create
// the cookie monster on that thread (or at least its store, i.e., the DB
// thread).

TEST_F(MultiThreadedCookieMonsterTest, ThreadCheckGetCookies) {
  scoped_refptr<CookieMonster> cm(new CookieMonster(NULL, NULL));
  EXPECT_TRUE(SetCookie(cm, url_google_, "A=B"));
  EXPECT_EQ("A=B", GetCookies(cm, url_google_));
  GetCookieStringCallback callback(&other_thread_);
  base::Closure task = base::Bind(
      &net::MultiThreadedCookieMonsterTest::GetCookiesTask,
      base::Unretained(this),
      cm, url_google_, &callback);
  RunOnOtherThread(task);
  EXPECT_TRUE(callback.did_run());
  EXPECT_EQ("A=B", callback.cookie());
}

TEST_F(MultiThreadedCookieMonsterTest, ThreadCheckGetCookiesWithOptions) {
  scoped_refptr<CookieMonster> cm(new CookieMonster(NULL, NULL));
  CookieOptions options;
  EXPECT_TRUE(SetCookie(cm, url_google_, "A=B"));
  EXPECT_EQ("A=B", GetCookiesWithOptions(cm, url_google_, options));
  GetCookieStringCallback callback(&other_thread_);
  base::Closure task = base::Bind(
      &net::MultiThreadedCookieMonsterTest::GetCookiesWithOptionsTask,
      base::Unretained(this),
      cm, url_google_, options, &callback);
  RunOnOtherThread(task);
  EXPECT_TRUE(callback.did_run());
  EXPECT_EQ("A=B", callback.cookie());
}

TEST_F(MultiThreadedCookieMonsterTest, ThreadCheckGetCookiesWithInfo) {
  scoped_refptr<CookieMonster> cm(new CookieMonster(NULL, NULL));
  CookieOptions options;
  std::string cookie_line;
  std::vector<CookieStore::CookieInfo> cookie_infos;
  EXPECT_TRUE(SetCookie(cm, url_google_, "A=B"));
  GetCookiesWithInfo(cm, url_google_, options, &cookie_line, &cookie_infos);
  EXPECT_EQ("A=B", cookie_line);
  EXPECT_EQ(1U, cookie_infos.size());
  EXPECT_EQ("A", cookie_infos[0].name);
  EXPECT_EQ("", cookie_infos[0].mac_key);
  EXPECT_EQ("", cookie_infos[0].mac_algorithm);
  GetCookiesWithInfoCallback callback(&other_thread_);
  base::Closure task =
      base::Bind(&net::MultiThreadedCookieMonsterTest::GetCookiesWithInfoTask,
                 base::Unretained(this),
                 cm, url_google_, options, &callback);
  RunOnOtherThread(task);
  EXPECT_TRUE(callback.did_run());
  EXPECT_EQ("A=B", callback.cookie_line());
  EXPECT_EQ(1U, callback.cookie_info().size());
  EXPECT_EQ("A", callback.cookie_info()[0].name);
  EXPECT_EQ("", callback.cookie_info()[0].mac_key);
  EXPECT_EQ("", callback.cookie_info()[0].mac_algorithm);
}

TEST_F(MultiThreadedCookieMonsterTest, ThreadCheckGetAllCookies) {
  scoped_refptr<CookieMonster> cm(new CookieMonster(NULL, NULL));
  EXPECT_TRUE(SetCookie(cm, url_google_, "A=B"));
  CookieList cookies = GetAllCookies(cm);
  CookieList::const_iterator it = cookies.begin();
  ASSERT_TRUE(it != cookies.end());
  EXPECT_EQ("www.google.izzle", it->Domain());
  EXPECT_EQ("A", it->Name());
  ASSERT_TRUE(++it == cookies.end());
  GetCookieListCallback callback(&other_thread_);
  base::Closure task =
      base::Bind(&net::MultiThreadedCookieMonsterTest::GetAllCookiesTask,
                 base::Unretained(this),
                 cm, &callback);
  RunOnOtherThread(task);
  EXPECT_TRUE(callback.did_run());
  it = callback.cookies().begin();
  ASSERT_TRUE(it != callback.cookies().end());
  EXPECT_EQ("www.google.izzle", it->Domain());
  EXPECT_EQ("A", it->Name());
  ASSERT_TRUE(++it == callback.cookies().end());
}

TEST_F(MultiThreadedCookieMonsterTest, ThreadCheckGetAllCookiesForURL) {
  scoped_refptr<CookieMonster> cm(new CookieMonster(NULL, NULL));
  EXPECT_TRUE(SetCookie(cm, url_google_, "A=B"));
  CookieList cookies = GetAllCookiesForURL(cm, url_google_);
  CookieList::const_iterator it = cookies.begin();
  ASSERT_TRUE(it != cookies.end());
  EXPECT_EQ("www.google.izzle", it->Domain());
  EXPECT_EQ("A", it->Name());
  ASSERT_TRUE(++it == cookies.end());
  GetCookieListCallback callback(&other_thread_);
  base::Closure task =
      base::Bind(&net::MultiThreadedCookieMonsterTest::GetAllCookiesForURLTask,
                 base::Unretained(this),
                 cm, url_google_, &callback);
  RunOnOtherThread(task);
  EXPECT_TRUE(callback.did_run());
  it = callback.cookies().begin();
  ASSERT_TRUE(it != callback.cookies().end());
  EXPECT_EQ("www.google.izzle", it->Domain());
  EXPECT_EQ("A", it->Name());
  ASSERT_TRUE(++it == callback.cookies().end());
}

TEST_F(MultiThreadedCookieMonsterTest, ThreadCheckGetAllCookiesForURLWithOpt) {
  scoped_refptr<CookieMonster> cm(new CookieMonster(NULL, NULL));
  EXPECT_TRUE(SetCookie(cm, url_google_, "A=B"));
  CookieOptions options;
  CookieList cookies =
      GetAllCookiesForURLWithOptions(cm, url_google_, options);
  CookieList::const_iterator it = cookies.begin();
  ASSERT_TRUE(it != cookies.end());
  EXPECT_EQ("www.google.izzle", it->Domain());
  EXPECT_EQ("A", it->Name());
  ASSERT_TRUE(++it == cookies.end());
  GetCookieListCallback callback(&other_thread_);
  base::Closure task = base::Bind(
      &net::MultiThreadedCookieMonsterTest::GetAllCookiesForURLWithOptionsTask,
      base::Unretained(this),
      cm, url_google_, options, &callback);
  RunOnOtherThread(task);
  EXPECT_TRUE(callback.did_run());
  it = callback.cookies().begin();
  ASSERT_TRUE(it != callback.cookies().end());
  EXPECT_EQ("www.google.izzle", it->Domain());
  EXPECT_EQ("A", it->Name());
  ASSERT_TRUE(++it == callback.cookies().end());
}

TEST_F(MultiThreadedCookieMonsterTest, ThreadCheckSetCookieWithOptions) {
  scoped_refptr<CookieMonster> cm(new CookieMonster(NULL, NULL));
  CookieOptions options;
  EXPECT_TRUE(SetCookieWithOptions(cm, url_google_, "A=B", options));
  SetCookieCallback callback(&other_thread_);
  base::Closure task = base::Bind(
      &net::MultiThreadedCookieMonsterTest::SetCookieWithOptionsTask,
      base::Unretained(this),
      cm, url_google_, "A=B", options, &callback);
  RunOnOtherThread(task);
  EXPECT_TRUE(callback.did_run());
  EXPECT_TRUE(callback.result());
}

TEST_F(MultiThreadedCookieMonsterTest, ThreadCheckSetCookieWithDetails) {
  scoped_refptr<CookieMonster> cm(new CookieMonster(NULL, NULL));
  EXPECT_TRUE(SetCookieWithDetails(
      cm, url_google_foo_,
      "A", "B", std::string(), "/foo", base::Time(),
      false, false));
  SetCookieCallback callback(&other_thread_);
  base::Closure task = base::Bind(
      &net::MultiThreadedCookieMonsterTest::SetCookieWithDetailsTask,
      base::Unretained(this),
      cm, url_google_foo_, &callback);
  RunOnOtherThread(task);
  EXPECT_TRUE(callback.did_run());
  EXPECT_TRUE(callback.result());
}

TEST_F(MultiThreadedCookieMonsterTest, ThreadCheckDeleteCookie) {
  scoped_refptr<CookieMonster> cm(new CookieMonster(NULL, NULL));
  CookieOptions options;
  EXPECT_TRUE(SetCookieWithOptions(cm, url_google_, "A=B", options));
  DeleteCookie(cm, url_google_, "A");
  EXPECT_TRUE(SetCookieWithOptions(cm, url_google_, "A=B", options));
  DeleteCookieCallback callback(&other_thread_);
  base::Closure task = base::Bind(
      &net::MultiThreadedCookieMonsterTest::DeleteCookieTask,
      base::Unretained(this),
      cm, url_google_, "A", &callback);
  RunOnOtherThread(task);
  EXPECT_TRUE(callback.did_run());
}

TEST_F(MultiThreadedCookieMonsterTest, ThreadCheckDeleteAllCreatedBetween) {
  scoped_refptr<CookieMonster> cm(new CookieMonster(NULL, NULL));
  CookieOptions options;
  Time now = Time::Now();
  EXPECT_TRUE(SetCookieWithOptions(cm, url_google_, "A=B", options));
  EXPECT_EQ(1, DeleteAllCreatedBetween(cm, now - TimeDelta::FromDays(99),
                                       Time()));
  EXPECT_TRUE(SetCookieWithOptions(cm, url_google_, "A=B", options));
  DeleteCallback callback(&other_thread_);
  base::Closure task = base::Bind(
      &net::MultiThreadedCookieMonsterTest::DeleteAllCreatedBetweenTask,
      base::Unretained(this),
      cm, now - TimeDelta::FromDays(99),
      Time(), &callback);
  RunOnOtherThread(task);
  EXPECT_TRUE(callback.did_run());
  EXPECT_EQ(1, callback.num_deleted());
}

TEST_F(MultiThreadedCookieMonsterTest, ThreadCheckDeleteAllForHost) {
  scoped_refptr<CookieMonster> cm(new CookieMonster(NULL, NULL));
  CookieOptions options;
  EXPECT_TRUE(SetCookieWithOptions(cm, url_google_, "A=B", options));
  EXPECT_EQ(1, DeleteAllForHost(cm, url_google_));
  EXPECT_TRUE(SetCookieWithOptions(cm, url_google_, "A=B", options));
  DeleteCallback callback(&other_thread_);
  base::Closure task = base::Bind(
      &net::MultiThreadedCookieMonsterTest::DeleteAllForHostTask,
      base::Unretained(this),
      cm, url_google_, &callback);
  RunOnOtherThread(task);
  EXPECT_TRUE(callback.did_run());
  EXPECT_EQ(1, callback.num_deleted());
}

TEST_F(MultiThreadedCookieMonsterTest, ThreadCheckDeleteCanonicalCookie) {
  scoped_refptr<CookieMonster> cm(new CookieMonster(NULL, NULL));
  CookieOptions options;
  EXPECT_TRUE(SetCookieWithOptions(cm, url_google_, "A=B", options));
  CookieList cookies = GetAllCookies(cm);
  CookieList::iterator it = cookies.begin();
  EXPECT_TRUE(DeleteCanonicalCookie(cm, *it));

  EXPECT_TRUE(SetCookieWithOptions(cm, url_google_, "A=B", options));
  SetCookieCallback callback(&other_thread_);
  cookies = GetAllCookies(cm);
  it = cookies.begin();
  base::Closure task = base::Bind(
      &net::MultiThreadedCookieMonsterTest::DeleteCanonicalCookieTask,
      base::Unretained(this),
      cm, *it, &callback);
  RunOnOtherThread(task);
  EXPECT_TRUE(callback.did_run());
  EXPECT_TRUE(callback.result());
}

TEST_F(CookieMonsterTest, ShortLivedSessionCookies) {
  scoped_refptr<MockPersistentCookieStore> store(
      new MockPersistentCookieStore);
  scoped_refptr<CookieMonster> cm(new CookieMonster(store, NULL));

  // Create a short-lived session cookie.
  CookieOptions options;
  options.set_force_session();
  Time current = Time::Now();
  EXPECT_TRUE(SetCookieWithOptions(cm, url_google_,
                                   std::string(kValidCookieLine) +
                                   "; max-age=10",
                                   options));

  // FindCookiesForKey asserts that its caller holds this lock.
  base::AutoLock auto_lock(cm->lock_);

  // Get cookies before the cookie has expired.
  std::vector<CookieMonster::CanonicalCookie*> cookies;
  cm->FindCookiesForKey(cm->GetKey(url_google_.host()), url_google_,
                        CookieOptions(), current, false, &cookies);
  EXPECT_EQ(1U, cookies.size());

  // Get cookies after the cookie has expired.
  cookies.clear();
  cm->FindCookiesForKey(cm->GetKey(url_google_.host()), url_google_,
                        CookieOptions(), current + TimeDelta::FromSeconds(20),
                        false, &cookies);
  EXPECT_EQ(0U, cookies.size());
}

TEST_F(CookieMonsterTest, InvalidExpiryTime) {
  CookieMonster::ParsedCookie pc(
      std::string(kValidCookieLine) + "; expires=Blarg arg arg");
  scoped_ptr<CookieMonster::CanonicalCookie> cookie(
      CookieMonster::CanonicalCookie::Create(url_google_, pc));

  ASSERT_FALSE(cookie->DoesExpire());
}

// Test that CookieMonster writes session cookies into the underlying
// CookieStore if the "persist session cookies" option is on.
TEST_F(CookieMonsterTest, PersistSessionCookies) {
  scoped_refptr<MockPersistentCookieStore> store(
      new MockPersistentCookieStore);
  scoped_refptr<CookieMonster> cm(new CookieMonster(store, NULL));
  cm->SetPersistSessionCookies(true);

  // All cookies set with SetCookie are session cookies.
  EXPECT_TRUE(SetCookie(cm, url_google_, "A=B"));
  EXPECT_EQ("A=B", GetCookies(cm, url_google_));

  // The cookie was written to the backing store.
  EXPECT_EQ(1u, store->commands().size());
  EXPECT_EQ(CookieStoreCommand::ADD, store->commands()[0].type);
  EXPECT_EQ("A", store->commands()[0].cookie.Name());
  EXPECT_EQ("B", store->commands()[0].cookie.Value());

  // Modify the cookie.
  EXPECT_TRUE(SetCookie(cm, url_google_, "A=C"));
  EXPECT_EQ("A=C", GetCookies(cm, url_google_));
  EXPECT_EQ(3u, store->commands().size());
  EXPECT_EQ(CookieStoreCommand::REMOVE, store->commands()[1].type);
  EXPECT_EQ("A", store->commands()[1].cookie.Name());
  EXPECT_EQ("B", store->commands()[1].cookie.Value());
  EXPECT_EQ(CookieStoreCommand::ADD, store->commands()[2].type);
  EXPECT_EQ("A", store->commands()[2].cookie.Name());
  EXPECT_EQ("C", store->commands()[2].cookie.Value());

  // Delete the cookie.
  DeleteCookie(cm, url_google_, "A");
  EXPECT_EQ("", GetCookies(cm, url_google_));
  EXPECT_EQ(4u, store->commands().size());
  EXPECT_EQ(CookieStoreCommand::REMOVE, store->commands()[3].type);
  EXPECT_EQ("A", store->commands()[3].cookie.Name());
  EXPECT_EQ("C", store->commands()[3].cookie.Value());
}

}  // namespace net
