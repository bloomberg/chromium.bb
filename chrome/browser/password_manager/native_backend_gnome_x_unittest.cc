// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdarg.h>

#include "base/basictypes.h"
#include "base/prefs/pref_service.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/password_manager/native_backend_gnome_x.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/psl_matching_helper.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

using autofill::PasswordForm;
using base::UTF8ToUTF16;
using base::UTF16ToUTF8;
using content::BrowserThread;

namespace {

// What follows is a very simple implementation of the subset of the GNOME
// Keyring API that we actually use. It gets substituted for the real one by
// MockGnomeKeyringLoader, which hooks into the facility normally used to load
// the GNOME Keyring library at runtime to avoid a static dependency on it.

struct MockKeyringItem {
  MockKeyringItem() {}
  MockKeyringItem(const char* keyring,
                  const std::string& display_name,
                  const std::string& password)
    : keyring(keyring ? keyring : "login"),
      display_name(display_name),
      password(password) {}

  struct ItemAttribute {
    ItemAttribute() : type(UINT32), value_uint32(0) {}
    explicit ItemAttribute(uint32_t value)
      : type(UINT32), value_uint32(value) {}
    explicit ItemAttribute(const std::string& value)
      : type(STRING), value_string(value) {}

    bool Equals(const ItemAttribute& x) const {
      if (type != x.type) return false;
      return (type == STRING) ? value_string == x.value_string
                              : value_uint32 == x.value_uint32;
    }

    enum Type { UINT32, STRING } type;
    uint32_t value_uint32;
    std::string value_string;
  };

  typedef std::map<std::string, ItemAttribute> attribute_map;
  typedef std::vector<std::pair<std::string, ItemAttribute> > attribute_query;

  bool Matches(const attribute_query& query) const {
    // The real GNOME Keyring doesn't match empty queries.
    if (query.empty()) return false;
    for (size_t i = 0; i < query.size(); ++i) {
      attribute_map::const_iterator match = attributes.find(query[i].first);
      if (match == attributes.end()) return false;
      if (!match->second.Equals(query[i].second)) return false;
    }
    return true;
  }

  std::string keyring;
  std::string display_name;
  std::string password;

  attribute_map attributes;
};

// The list of all keyring items we have stored.
std::vector<MockKeyringItem> mock_keyring_items;
bool mock_keyring_reject_local_ids = false;

bool IsStringAttribute(const GnomeKeyringPasswordSchema* schema,
                       const std::string& name) {
  for (size_t i = 0; schema->attributes[i].name; ++i)
    if (name == schema->attributes[i].name)
      return schema->attributes[i].type == GNOME_KEYRING_ATTRIBUTE_TYPE_STRING;
  NOTREACHED() << "Requested type of nonexistent attribute";
  return false;
}

gboolean mock_gnome_keyring_is_available() {
  return true;
}

gpointer mock_gnome_keyring_store_password(
    const GnomeKeyringPasswordSchema* schema,
    const gchar* keyring,
    const gchar* display_name,
    const gchar* password,
    GnomeKeyringOperationDoneCallback callback,
    gpointer data,
    GDestroyNotify destroy_data,
    ...) {
  mock_keyring_items.push_back(
      MockKeyringItem(keyring, display_name, password));
  MockKeyringItem* item = &mock_keyring_items.back();
  const std::string keyring_desc =
      keyring ? base::StringPrintf("keyring %s", keyring)
              : std::string("default keyring");
  VLOG(1) << "Adding item with origin " << display_name
          << " to " << keyring_desc;
  va_list ap;
  va_start(ap, destroy_data);
  char* name;
  while ((name = va_arg(ap, gchar*))) {
    if (IsStringAttribute(schema, name)) {
      item->attributes[name] =
          MockKeyringItem::ItemAttribute(va_arg(ap, gchar*));
      VLOG(1) << "Adding item attribute " << name
              << ", value '" << item->attributes[name].value_string << "'";
    } else {
      item->attributes[name] =
          MockKeyringItem::ItemAttribute(va_arg(ap, uint32_t));
      VLOG(1) << "Adding item attribute " << name
              << ", value " << item->attributes[name].value_uint32;
    }
  }
  va_end(ap);
  // As a hack to ease testing migration, make it possible to reject the new
  // format for the app string. This way we can add them easily to migrate.
  if (mock_keyring_reject_local_ids) {
    MockKeyringItem::attribute_map::iterator it =
        item->attributes.find("application");
    if (it != item->attributes.end() &&
        it->second.type == MockKeyringItem::ItemAttribute::STRING &&
        base::StringPiece(it->second.value_string).starts_with("chrome-")) {
      mock_keyring_items.pop_back();
      // GnomeKeyringResult, data
      callback(GNOME_KEYRING_RESULT_IO_ERROR, data);
      return NULL;
    }
  }
  // GnomeKeyringResult, data
  callback(GNOME_KEYRING_RESULT_OK, data);
  return NULL;
}

gpointer mock_gnome_keyring_delete_password(
    const GnomeKeyringPasswordSchema* schema,
    GnomeKeyringOperationDoneCallback callback,
    gpointer data,
    GDestroyNotify destroy_data,
    ...) {
  MockKeyringItem::attribute_query query;
  va_list ap;
  va_start(ap, destroy_data);
  char* name;
  while ((name = va_arg(ap, gchar*))) {
    if (IsStringAttribute(schema, name)) {
      query.push_back(make_pair(std::string(name),
          MockKeyringItem::ItemAttribute(va_arg(ap, gchar*))));
      VLOG(1) << "Querying with item attribute " << name
              << ", value '" << query.back().second.value_string << "'";
    } else {
      query.push_back(make_pair(std::string(name),
          MockKeyringItem::ItemAttribute(va_arg(ap, uint32_t))));
      VLOG(1) << "Querying with item attribute " << name
              << ", value " << query.back().second.value_uint32;
    }
  }
  va_end(ap);
  bool deleted = false;
  for (size_t i = mock_keyring_items.size(); i > 0; --i) {
    const MockKeyringItem* item = &mock_keyring_items[i - 1];
    if (item->Matches(query)) {
      VLOG(1) << "Deleting item with origin " <<  item->display_name;
      mock_keyring_items.erase(mock_keyring_items.begin() + (i - 1));
      deleted = true;
    }
  }
  // GnomeKeyringResult, data
  callback(deleted ? GNOME_KEYRING_RESULT_OK
                   : GNOME_KEYRING_RESULT_NO_MATCH, data);
  return NULL;
}

gpointer mock_gnome_keyring_find_items(
    GnomeKeyringItemType type,
    GnomeKeyringAttributeList* attributes,
    GnomeKeyringOperationGetListCallback callback,
    gpointer data,
    GDestroyNotify destroy_data) {
  MockKeyringItem::attribute_query query;
  for (size_t i = 0; i < attributes->len; ++i) {
    GnomeKeyringAttribute attribute =
        g_array_index(attributes, GnomeKeyringAttribute, i);
    if (attribute.type == GNOME_KEYRING_ATTRIBUTE_TYPE_STRING) {
      query.push_back(
          make_pair(std::string(attribute.name),
                    MockKeyringItem::ItemAttribute(attribute.value.string)));
      VLOG(1) << "Querying with item attribute " << attribute.name
              << ", value '" << query.back().second.value_string << "'";
    } else {
      query.push_back(
          make_pair(std::string(attribute.name),
                    MockKeyringItem::ItemAttribute(attribute.value.integer)));
      VLOG(1) << "Querying with item attribute " << attribute.name << ", value "
              << query.back().second.value_uint32;
    }
  }
  // Find matches and add them to a list of results.
  GList* results = NULL;
  for (size_t i = 0; i < mock_keyring_items.size(); ++i) {
    const MockKeyringItem* item = &mock_keyring_items[i];
    if (item->Matches(query)) {
      GnomeKeyringFound* found = new GnomeKeyringFound;
      found->keyring = strdup(item->keyring.c_str());
      found->item_id = i;
      found->attributes = gnome_keyring_attribute_list_new();
      for (MockKeyringItem::attribute_map::const_iterator it =
               item->attributes.begin();
           it != item->attributes.end();
           ++it) {
        if (it->second.type == MockKeyringItem::ItemAttribute::STRING) {
          gnome_keyring_attribute_list_append_string(
              found->attributes, it->first.c_str(),
              it->second.value_string.c_str());
        } else {
          gnome_keyring_attribute_list_append_uint32(
              found->attributes, it->first.c_str(),
              it->second.value_uint32);
        }
      }
      found->secret = strdup(item->password.c_str());
      results = g_list_prepend(results, found);
    }
  }
  // GnomeKeyringResult, GList*, data
  callback(results ? GNOME_KEYRING_RESULT_OK
                   : GNOME_KEYRING_RESULT_NO_MATCH, results, data);
  // Now free the list of results.
  GList* element = g_list_first(results);
  while (element) {
    GnomeKeyringFound* found = static_cast<GnomeKeyringFound*>(element->data);
    free(found->keyring);
    gnome_keyring_attribute_list_free(found->attributes);
    free(found->secret);
    delete found;
    element = g_list_next(element);
  }
  g_list_free(results);
  return NULL;
}

const gchar* mock_gnome_keyring_result_to_message(GnomeKeyringResult res) {
  return "mock keyring simulating failure";
}

// Inherit to get access to protected fields.
class MockGnomeKeyringLoader : public GnomeKeyringLoader {
 public:
  static bool LoadMockGnomeKeyring() {
    if (!LoadGnomeKeyring())
      return false;
#define GNOME_KEYRING_ASSIGN_POINTER(name) \
  gnome_keyring_##name = &mock_gnome_keyring_##name;
    GNOME_KEYRING_FOR_EACH_MOCKED_FUNC(GNOME_KEYRING_ASSIGN_POINTER)
#undef GNOME_KEYRING_ASSIGN_POINTER
    keyring_loaded = true;
    // Reset the state of the mock library.
    mock_keyring_items.clear();
    mock_keyring_reject_local_ids = false;
    return true;
  }
};

}  // anonymous namespace

class NativeBackendGnomeTest : public testing::Test {
 protected:
  NativeBackendGnomeTest()
      : ui_thread_(BrowserThread::UI, &message_loop_),
        db_thread_(BrowserThread::DB) {
  }

  virtual void SetUp() {
    ASSERT_TRUE(db_thread_.Start());

    ASSERT_TRUE(MockGnomeKeyringLoader::LoadMockGnomeKeyring());

    form_google_.origin = GURL("http://www.google.com/");
    form_google_.action = GURL("http://www.google.com/login");
    form_google_.username_element = UTF8ToUTF16("user");
    form_google_.username_value = UTF8ToUTF16("joeschmoe");
    form_google_.password_element = UTF8ToUTF16("pass");
    form_google_.password_value = UTF8ToUTF16("seekrit");
    form_google_.submit_element = UTF8ToUTF16("submit");
    form_google_.signon_realm = "http://www.google.com/";
    form_google_.type = PasswordForm::TYPE_GENERATED;

    form_facebook_.origin = GURL("http://www.facebook.com/");
    form_facebook_.action = GURL("http://www.facebook.com/login");
    form_facebook_.username_element = UTF8ToUTF16("user");
    form_facebook_.username_value = UTF8ToUTF16("a");
    form_facebook_.password_element = UTF8ToUTF16("password");
    form_facebook_.password_value = UTF8ToUTF16("b");
    form_facebook_.submit_element = UTF8ToUTF16("submit");
    form_facebook_.signon_realm = "http://www.facebook.com/";

    form_isc_.origin = GURL("http://www.isc.org/");
    form_isc_.action = GURL("http://www.isc.org/auth");
    form_isc_.username_element = UTF8ToUTF16("id");
    form_isc_.username_value = UTF8ToUTF16("janedoe");
    form_isc_.password_element = UTF8ToUTF16("passwd");
    form_isc_.password_value = UTF8ToUTF16("ihazabukkit");
    form_isc_.submit_element = UTF8ToUTF16("login");
    form_isc_.signon_realm = "http://www.isc.org/";
  }

  virtual void TearDown() {
    base::MessageLoop::current()->PostTask(FROM_HERE,
                                           base::MessageLoop::QuitClosure());
    base::MessageLoop::current()->Run();
    db_thread_.Stop();
  }

  void RunBothThreads() {
    // First we post a message to the DB thread that will run after all other
    // messages that have been posted to the DB thread (we don't expect more
    // to be posted), which posts a message to the UI thread to quit the loop.
    // That way we can run both loops and be sure that the UI thread loop will
    // quit so we can get on with the rest of the test.
    BrowserThread::PostTask(BrowserThread::DB, FROM_HERE,
        base::Bind(&PostQuitTask, &message_loop_));
    base::MessageLoop::current()->Run();
  }

  static void PostQuitTask(base::MessageLoop* loop) {
    loop->PostTask(FROM_HERE, base::MessageLoop::QuitClosure());
  }

  void CheckUint32Attribute(const MockKeyringItem* item,
                            const std::string& attribute,
                            uint32_t value) {
    MockKeyringItem::attribute_map::const_iterator it =
        item->attributes.find(attribute);
    EXPECT_NE(item->attributes.end(), it);
    if (it != item->attributes.end()) {
      EXPECT_EQ(MockKeyringItem::ItemAttribute::UINT32, it->second.type);
      EXPECT_EQ(value, it->second.value_uint32);
    }
  }

  void CheckStringAttribute(const MockKeyringItem* item,
                            const std::string& attribute,
                            const std::string& value) {
    MockKeyringItem::attribute_map::const_iterator it =
        item->attributes.find(attribute);
    EXPECT_NE(item->attributes.end(), it);
    if (it != item->attributes.end()) {
      EXPECT_EQ(MockKeyringItem::ItemAttribute::STRING, it->second.type);
      EXPECT_EQ(value, it->second.value_string);
    }
  }

  void CheckMockKeyringItem(const MockKeyringItem* item,
                            const PasswordForm& form,
                            const std::string& app_string) {
    // We always add items to the login keyring.
    EXPECT_EQ("login", item->keyring);
    EXPECT_EQ(form.origin.spec(), item->display_name);
    EXPECT_EQ(UTF16ToUTF8(form.password_value), item->password);
    EXPECT_EQ(15u, item->attributes.size());
    CheckStringAttribute(item, "origin_url", form.origin.spec());
    CheckStringAttribute(item, "action_url", form.action.spec());
    CheckStringAttribute(item, "username_element",
                         UTF16ToUTF8(form.username_element));
    CheckStringAttribute(item, "username_value",
                         UTF16ToUTF8(form.username_value));
    CheckStringAttribute(item, "password_element",
                         UTF16ToUTF8(form.password_element));
    CheckStringAttribute(item, "submit_element",
                         UTF16ToUTF8(form.submit_element));
    CheckStringAttribute(item, "signon_realm", form.signon_realm);
    CheckUint32Attribute(item, "ssl_valid", form.ssl_valid);
    CheckUint32Attribute(item, "preferred", form.preferred);
    // We don't check the date created. It varies.
    CheckUint32Attribute(item, "blacklisted_by_user", form.blacklisted_by_user);
    CheckUint32Attribute(item, "type", form.type);
    CheckUint32Attribute(item, "times_used", form.times_used);
    CheckUint32Attribute(item, "scheme", form.scheme);
    CheckStringAttribute(item, "application", app_string);
  }

  // Checks (using EXPECT_* macros), that |credentials| are accessible for
  // filling in for a page with |origin| iff
  // |should_credential_be_available_to_url| is true.
  void CheckCredentialAvailability(const PasswordForm& credentials,
                                   const std::string& url,
                                   bool should_credential_be_available_to_url) {
    password_manager::PSLMatchingHelper helper;
    ASSERT_TRUE(helper.IsMatchingEnabled())
        << "PSL matching needs to be enabled.";

    NativeBackendGnome backend(321);
    backend.Init();

    BrowserThread::PostTask(
        BrowserThread::DB,
        FROM_HERE,
        base::Bind(base::IgnoreResult(&NativeBackendGnome::AddLogin),
                   base::Unretained(&backend),
                   credentials));

    PasswordForm target_form;
    target_form.origin = GURL(url);
    target_form.signon_realm = url;
    std::vector<PasswordForm*> form_list;
    BrowserThread::PostTask(
        BrowserThread::DB,
        FROM_HERE,
        base::Bind(base::IgnoreResult(&NativeBackendGnome::GetLogins),
                   base::Unretained(&backend),
                   target_form,
                   &form_list));

    RunBothThreads();

    EXPECT_EQ(1u, mock_keyring_items.size());
    if (mock_keyring_items.size() > 0)
      CheckMockKeyringItem(&mock_keyring_items[0], credentials, "chrome-321");

    if (should_credential_be_available_to_url)
      EXPECT_EQ(1u, form_list.size());
    else
      EXPECT_EQ(0u, form_list.size());
    STLDeleteElements(&form_list);
  }

  base::MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread db_thread_;

  // Provide some test forms to avoid having to set them up in each test.
  PasswordForm form_google_;
  PasswordForm form_facebook_;
  PasswordForm form_isc_;
};

TEST_F(NativeBackendGnomeTest, BasicAddLogin) {
  NativeBackendGnome backend(42);
  backend.Init();

  BrowserThread::PostTask(
      BrowserThread::DB, FROM_HERE,
      base::Bind(base::IgnoreResult(&NativeBackendGnome::AddLogin),
                 base::Unretained(&backend), form_google_));

  RunBothThreads();

  EXPECT_EQ(1u, mock_keyring_items.size());
  if (mock_keyring_items.size() > 0)
    CheckMockKeyringItem(&mock_keyring_items[0], form_google_, "chrome-42");
}

TEST_F(NativeBackendGnomeTest, BasicListLogins) {
  NativeBackendGnome backend(42);
  backend.Init();

  BrowserThread::PostTask(
      BrowserThread::DB, FROM_HERE,
      base::Bind(base::IgnoreResult( &NativeBackendGnome::AddLogin),
                 base::Unretained(&backend), form_google_));

  std::vector<PasswordForm*> form_list;
  BrowserThread::PostTask(
      BrowserThread::DB, FROM_HERE,
      base::Bind(
          base::IgnoreResult(&NativeBackendGnome::GetAutofillableLogins),
          base::Unretained(&backend), &form_list));

  RunBothThreads();

  // Quick check that we got something back.
  EXPECT_EQ(1u, form_list.size());
  STLDeleteElements(&form_list);

  EXPECT_EQ(1u, mock_keyring_items.size());
  if (mock_keyring_items.size() > 0)
    CheckMockKeyringItem(&mock_keyring_items[0], form_google_, "chrome-42");
}

// Save a password for www.facebook.com and see it suggested for m.facebook.com.
TEST_F(NativeBackendGnomeTest, PSLMatchingPositive) {
  CheckCredentialAvailability(form_facebook_,
                              "http://m.facebook.com/",
                              /*should_credential_be_available_to_url=*/true);
}

// Save a password for www.facebook.com and see it not suggested for
// m-facebook.com.
TEST_F(NativeBackendGnomeTest, PSLMatchingNegativeDomainMismatch) {
  CheckCredentialAvailability(form_facebook_,
                              "http://m-facebook.com/",
                              /*should_credential_be_available_to_url=*/false);
}

// Test PSL matching is off for domains excluded from it.
TEST_F(NativeBackendGnomeTest, PSLMatchingDisabledDomains) {
  CheckCredentialAvailability(form_google_,
                              "http://one.google.com/",
                              /*should_credential_be_available_to_url=*/false);
}

TEST_F(NativeBackendGnomeTest, BasicUpdateLogin) {
  NativeBackendGnome backend(42);
  backend.Init();

  // First add google login.
  BrowserThread::PostTask(
      BrowserThread::DB, FROM_HERE,
      base::Bind(base::IgnoreResult(&NativeBackendGnome::AddLogin),
                 base::Unretained(&backend), form_google_));

  RunBothThreads();

  PasswordForm new_form_google(form_google_);
  new_form_google.times_used = 1;
  new_form_google.action = GURL("http://www.google.com/different/login");

  EXPECT_EQ(1u, mock_keyring_items.size());
  if (mock_keyring_items.size() > 0)
    CheckMockKeyringItem(&mock_keyring_items[0], form_google_, "chrome-42");

  // Update login
  BrowserThread::PostTask(
      BrowserThread::DB, FROM_HERE,
      base::Bind(base::IgnoreResult(&NativeBackendGnome::UpdateLogin),
                 base::Unretained(&backend), new_form_google));

  RunBothThreads();

  EXPECT_EQ(1u, mock_keyring_items.size());
  if (mock_keyring_items.size() > 0)
    CheckMockKeyringItem(&mock_keyring_items[0], new_form_google, "chrome-42");
}

TEST_F(NativeBackendGnomeTest, BasicRemoveLogin) {
  NativeBackendGnome backend(42);
  backend.Init();

  BrowserThread::PostTask(
      BrowserThread::DB, FROM_HERE,
      base::Bind(base::IgnoreResult(&NativeBackendGnome::AddLogin),
                 base::Unretained(&backend), form_google_));

  RunBothThreads();

  EXPECT_EQ(1u, mock_keyring_items.size());
  if (mock_keyring_items.size() > 0)
    CheckMockKeyringItem(&mock_keyring_items[0], form_google_, "chrome-42");

  BrowserThread::PostTask(
      BrowserThread::DB, FROM_HERE,
      base::Bind(base::IgnoreResult(&NativeBackendGnome::RemoveLogin),
                 base::Unretained(&backend), form_google_));

  RunBothThreads();

  EXPECT_EQ(0u, mock_keyring_items.size());
}

TEST_F(NativeBackendGnomeTest, RemoveNonexistentLogin) {
  NativeBackendGnome backend(42);
  backend.Init();

  // First add an unrelated login.
  BrowserThread::PostTask(
      BrowserThread::DB, FROM_HERE,
      base::Bind(base::IgnoreResult(&NativeBackendGnome::AddLogin),
                 base::Unretained(&backend), form_google_));

  RunBothThreads();

  EXPECT_EQ(1u, mock_keyring_items.size());
  if (mock_keyring_items.size() > 0)
    CheckMockKeyringItem(&mock_keyring_items[0], form_google_, "chrome-42");

  // Attempt to remove a login that doesn't exist.
  BrowserThread::PostTask(
      BrowserThread::DB, FROM_HERE,
      base::Bind(base::IgnoreResult(&NativeBackendGnome::RemoveLogin),
                 base::Unretained(&backend), form_isc_));

  // Make sure we can still get the first form back.
  std::vector<PasswordForm*> form_list;
  BrowserThread::PostTask(
      BrowserThread::DB, FROM_HERE,
      base::Bind(
          base::IgnoreResult(&NativeBackendGnome::GetAutofillableLogins),
          base::Unretained(&backend), &form_list));

  RunBothThreads();

  // Quick check that we got something back.
  EXPECT_EQ(1u, form_list.size());
  STLDeleteElements(&form_list);

  EXPECT_EQ(1u, mock_keyring_items.size());
  if (mock_keyring_items.size() > 0)
    CheckMockKeyringItem(&mock_keyring_items[0], form_google_, "chrome-42");
}

TEST_F(NativeBackendGnomeTest, AddDuplicateLogin) {
  NativeBackendGnome backend(42);
  backend.Init();

  BrowserThread::PostTask(
      BrowserThread::DB, FROM_HERE,
      base::Bind(base::IgnoreResult(&NativeBackendGnome::AddLogin),
                 base::Unretained(&backend), form_google_));
  BrowserThread::PostTask(
      BrowserThread::DB, FROM_HERE,
      base::Bind(base::IgnoreResult(&NativeBackendGnome::AddLogin),
                 base::Unretained(&backend), form_google_));

  RunBothThreads();

  EXPECT_EQ(1u, mock_keyring_items.size());
  if (mock_keyring_items.size() > 0)
    CheckMockKeyringItem(&mock_keyring_items[0], form_google_, "chrome-42");
}

TEST_F(NativeBackendGnomeTest, ListLoginsAppends) {
  NativeBackendGnome backend(42);
  backend.Init();

  BrowserThread::PostTask(
      BrowserThread::DB, FROM_HERE,
      base::Bind(base::IgnoreResult(&NativeBackendGnome::AddLogin),
                 base::Unretained(&backend), form_google_));

  // Send the same request twice with the same list both times.
  std::vector<PasswordForm*> form_list;
  BrowserThread::PostTask(
      BrowserThread::DB, FROM_HERE,
      base::Bind(
          base::IgnoreResult(&NativeBackendGnome::GetAutofillableLogins),
          base::Unretained(&backend), &form_list));
  BrowserThread::PostTask(
      BrowserThread::DB, FROM_HERE,
      base::Bind(
          base::IgnoreResult(&NativeBackendGnome::GetAutofillableLogins),
          base::Unretained(&backend), &form_list));

  RunBothThreads();

  // Quick check that we got two results back.
  EXPECT_EQ(2u, form_list.size());
  STLDeleteElements(&form_list);

  EXPECT_EQ(1u, mock_keyring_items.size());
  if (mock_keyring_items.size() > 0)
    CheckMockKeyringItem(&mock_keyring_items[0], form_google_, "chrome-42");
}

// TODO(mdm): add more basic tests here at some point.
