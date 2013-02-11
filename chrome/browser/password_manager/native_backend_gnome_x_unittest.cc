// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdarg.h>

#include "base/basictypes.h"
#include "base/prefs/pref_service.h"
#include "base/stl_util.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/password_manager/native_backend_gnome_x.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;
using content::PasswordForm;

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

gpointer mock_gnome_keyring_find_itemsv(
    GnomeKeyringItemType type,
    GnomeKeyringOperationGetListCallback callback,
    gpointer data,
    GDestroyNotify destroy_data,
    ...) {
  MockKeyringItem::attribute_query query;
  va_list ap;
  va_start(ap, destroy_data);
  char* name;
  while ((name = va_arg(ap, gchar*))) {
    // Really a GnomeKeyringAttributeType, but promoted to int through ...
    if (va_arg(ap, int) == GNOME_KEYRING_ATTRIBUTE_TYPE_STRING) {
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
#define GNOME_KEYRING_ASSIGN_POINTER(name) \
  gnome_keyring_##name = &mock_gnome_keyring_##name;
    GNOME_KEYRING_FOR_EACH_FUNC(GNOME_KEYRING_ASSIGN_POINTER)
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

    MockGnomeKeyringLoader::LoadMockGnomeKeyring();

    form_google_.origin = GURL("http://www.google.com/");
    form_google_.action = GURL("http://www.google.com/login");
    form_google_.username_element = UTF8ToUTF16("user");
    form_google_.username_value = UTF8ToUTF16("joeschmoe");
    form_google_.password_element = UTF8ToUTF16("pass");
    form_google_.password_value = UTF8ToUTF16("seekrit");
    form_google_.submit_element = UTF8ToUTF16("submit");
    form_google_.signon_realm = "Google";

    form_isc_.origin = GURL("http://www.isc.org/");
    form_isc_.action = GURL("http://www.isc.org/auth");
    form_isc_.username_element = UTF8ToUTF16("id");
    form_isc_.username_value = UTF8ToUTF16("janedoe");
    form_isc_.password_element = UTF8ToUTF16("passwd");
    form_isc_.password_value = UTF8ToUTF16("ihazabukkit");
    form_isc_.submit_element = UTF8ToUTF16("login");
    form_isc_.signon_realm = "ISC";
  }

  virtual void TearDown() {
    MessageLoop::current()->PostTask(FROM_HERE, MessageLoop::QuitClosure());
    MessageLoop::current()->Run();
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
    MessageLoop::current()->Run();
  }

  static void PostQuitTask(MessageLoop* loop) {
    loop->PostTask(FROM_HERE, MessageLoop::QuitClosure());
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
    EXPECT_EQ(13u, item->attributes.size());
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
    CheckUint32Attribute(item, "scheme", form.scheme);
    CheckStringAttribute(item, "application", app_string);
  }

  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread db_thread_;

  TestingProfile profile_;

  // Provide some test forms to avoid having to set them up in each test.
  PasswordForm form_google_;
  PasswordForm form_isc_;
};

TEST_F(NativeBackendGnomeTest, BasicAddLogin) {
  // Pretend that the migration has already taken place.
  profile_.GetPrefs()->SetBoolean(prefs::kPasswordsUseLocalProfileId, true);

  NativeBackendGnome backend(42, profile_.GetPrefs());
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
  // Pretend that the migration has already taken place.
  profile_.GetPrefs()->SetBoolean(prefs::kPasswordsUseLocalProfileId, true);

  NativeBackendGnome backend(42, profile_.GetPrefs());
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

TEST_F(NativeBackendGnomeTest, BasicRemoveLogin) {
  // Pretend that the migration has already taken place.
  profile_.GetPrefs()->SetBoolean(prefs::kPasswordsUseLocalProfileId, true);

  NativeBackendGnome backend(42, profile_.GetPrefs());
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
  // Pretend that the migration has already taken place.
  profile_.GetPrefs()->SetBoolean(prefs::kPasswordsUseLocalProfileId, true);

  NativeBackendGnome backend(42, profile_.GetPrefs());
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
  // Pretend that the migration has already taken place.
  profile_.GetPrefs()->SetBoolean(prefs::kPasswordsUseLocalProfileId, true);

  NativeBackendGnome backend(42, profile_.GetPrefs());
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
  // Pretend that the migration has already taken place.
  profile_.GetPrefs()->SetBoolean(prefs::kPasswordsUseLocalProfileId, true);

  NativeBackendGnome backend(42, profile_.GetPrefs());
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

// TODO(mdm): add more basic (i.e. non-migration) tests here at some point.

TEST_F(NativeBackendGnomeTest, DISABLED_MigrateOneLogin) {
  // Reject attempts to migrate so we can populate the store.
  mock_keyring_reject_local_ids = true;

  {
    NativeBackendGnome backend(42, profile_.GetPrefs());
    backend.Init();

    BrowserThread::PostTask(BrowserThread::DB, FROM_HERE,
        base::Bind(base::IgnoreResult(&NativeBackendGnome::AddLogin),
                   base::Unretained(&backend), form_google_));

    // Make sure we can get the form back even when migration is failing.
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
  }

  EXPECT_EQ(1u, mock_keyring_items.size());
  if (mock_keyring_items.size() > 0)
    CheckMockKeyringItem(&mock_keyring_items[0], form_google_, "chrome");

  // Now allow the migration.
  mock_keyring_reject_local_ids = false;

  {
    NativeBackendGnome backend(42, profile_.GetPrefs());
    backend.Init();

    // This should not trigger migration because there will be no results.
    std::vector<PasswordForm*> form_list;
    BrowserThread::PostTask(
        BrowserThread::DB, FROM_HERE,
        base::Bind(base::IgnoreResult(&NativeBackendGnome::GetBlacklistLogins),
                   base::Unretained(&backend), &form_list));

    RunBothThreads();

    // Check that we got nothing back.
    EXPECT_EQ(0u, form_list.size());
    STLDeleteElements(&form_list);
  }

  // Check that the keyring is unmodified.
  EXPECT_EQ(1u, mock_keyring_items.size());
  if (mock_keyring_items.size() > 0)
    CheckMockKeyringItem(&mock_keyring_items[0], form_google_, "chrome");

  // Check that we haven't set the persistent preference.
  EXPECT_FALSE(
      profile_.GetPrefs()->GetBoolean(prefs::kPasswordsUseLocalProfileId));

  {
    NativeBackendGnome backend(42, profile_.GetPrefs());
    backend.Init();

    // Trigger the migration by looking something up.
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
  }

  EXPECT_EQ(2u, mock_keyring_items.size());
  if (mock_keyring_items.size() > 0)
    CheckMockKeyringItem(&mock_keyring_items[0], form_google_, "chrome");
  if (mock_keyring_items.size() > 1)
    CheckMockKeyringItem(&mock_keyring_items[1], form_google_, "chrome-42");

  // Check that we have set the persistent preference.
  EXPECT_TRUE(
      profile_.GetPrefs()->GetBoolean(prefs::kPasswordsUseLocalProfileId));
}

TEST_F(NativeBackendGnomeTest, DISABLED_MigrateToMultipleProfiles) {
  // Reject attempts to migrate so we can populate the store.
  mock_keyring_reject_local_ids = true;

  {
    NativeBackendGnome backend(42, profile_.GetPrefs());
    backend.Init();

    BrowserThread::PostTask(
        BrowserThread::DB, FROM_HERE,
        base::Bind(base::IgnoreResult(&NativeBackendGnome::AddLogin),
                   base::Unretained(&backend), form_google_));

    RunBothThreads();
  }

  EXPECT_EQ(1u, mock_keyring_items.size());
  if (mock_keyring_items.size() > 0)
    CheckMockKeyringItem(&mock_keyring_items[0], form_google_, "chrome");

  // Now allow the migration.
  mock_keyring_reject_local_ids = false;

  {
    NativeBackendGnome backend(42, profile_.GetPrefs());
    backend.Init();

    // Trigger the migration by looking something up.
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
  }

  EXPECT_EQ(2u, mock_keyring_items.size());
  if (mock_keyring_items.size() > 0)
    CheckMockKeyringItem(&mock_keyring_items[0], form_google_, "chrome");
  if (mock_keyring_items.size() > 1)
    CheckMockKeyringItem(&mock_keyring_items[1], form_google_, "chrome-42");

  // Check that we have set the persistent preference.
  EXPECT_TRUE(
      profile_.GetPrefs()->GetBoolean(prefs::kPasswordsUseLocalProfileId));

  // Normally we'd actually have a different profile. But in the test just reset
  // the profile's persistent pref; we pass in the local profile id anyway.
  profile_.GetPrefs()->SetBoolean(prefs::kPasswordsUseLocalProfileId, false);

  {
    NativeBackendGnome backend(24, profile_.GetPrefs());
    backend.Init();

    // Trigger the migration by looking something up.
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
  }

  EXPECT_EQ(3u, mock_keyring_items.size());
  if (mock_keyring_items.size() > 0)
    CheckMockKeyringItem(&mock_keyring_items[0], form_google_, "chrome");
  if (mock_keyring_items.size() > 1)
    CheckMockKeyringItem(&mock_keyring_items[1], form_google_, "chrome-42");
  if (mock_keyring_items.size() > 2)
    CheckMockKeyringItem(&mock_keyring_items[2], form_google_, "chrome-24");
}

TEST_F(NativeBackendGnomeTest, DISABLED_NoMigrationWithPrefSet) {
  // Reject attempts to migrate so we can populate the store.
  mock_keyring_reject_local_ids = true;

  {
    NativeBackendGnome backend(42, profile_.GetPrefs());
    backend.Init();

    BrowserThread::PostTask(
        BrowserThread::DB, FROM_HERE,
        base::Bind(base::IgnoreResult(&NativeBackendGnome::AddLogin),
                   base::Unretained(&backend), form_google_));

    RunBothThreads();
  }

  EXPECT_EQ(1u, mock_keyring_items.size());
  if (mock_keyring_items.size() > 0)
    CheckMockKeyringItem(&mock_keyring_items[0], form_google_, "chrome");

  // Now allow migration, but also pretend that the it has already taken place.
  mock_keyring_reject_local_ids = false;
  profile_.GetPrefs()->SetBoolean(prefs::kPasswordsUseLocalProfileId, true);

  {
    NativeBackendGnome backend(42, profile_.GetPrefs());
    backend.Init();

    // Trigger the migration by adding a new login.
    BrowserThread::PostTask(
        BrowserThread::DB, FROM_HERE,
        base::Bind(base::IgnoreResult(&NativeBackendGnome::AddLogin),
                   base::Unretained(&backend), form_isc_));

    // Look up all logins; we expect only the one we added.
    std::vector<PasswordForm*> form_list;
    BrowserThread::PostTask(
        BrowserThread::DB, FROM_HERE,
        base::Bind(
            base::IgnoreResult(&NativeBackendGnome::GetAutofillableLogins),
            base::Unretained(&backend), &form_list));

    RunBothThreads();

    // Quick check that we got the right thing back.
    EXPECT_EQ(1u, form_list.size());
    if (form_list.size() > 0)
      EXPECT_EQ(form_isc_.signon_realm, form_list[0]->signon_realm);
    STLDeleteElements(&form_list);
  }

  EXPECT_EQ(2u, mock_keyring_items.size());
  if (mock_keyring_items.size() > 0)
    CheckMockKeyringItem(&mock_keyring_items[0], form_google_, "chrome");
  if (mock_keyring_items.size() > 1)
    CheckMockKeyringItem(&mock_keyring_items[1], form_isc_, "chrome-42");
}

TEST_F(NativeBackendGnomeTest, DISABLED_DeleteMigratedPasswordIsIsolated) {
  // Reject attempts to migrate so we can populate the store.
  mock_keyring_reject_local_ids = true;

  {
    NativeBackendGnome backend(42, profile_.GetPrefs());
    backend.Init();

    BrowserThread::PostTask(
        BrowserThread::DB, FROM_HERE,
        base::Bind(base::IgnoreResult(&NativeBackendGnome::AddLogin),
                   base::Unretained(&backend), form_google_));

    RunBothThreads();
  }

  EXPECT_EQ(1u, mock_keyring_items.size());
  if (mock_keyring_items.size() > 0)
    CheckMockKeyringItem(&mock_keyring_items[0], form_google_, "chrome");

  // Now allow the migration.
  mock_keyring_reject_local_ids = false;

  {
    NativeBackendGnome backend(42, profile_.GetPrefs());
    backend.Init();

    // Trigger the migration by looking something up.
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
  }

  EXPECT_EQ(2u, mock_keyring_items.size());
  if (mock_keyring_items.size() > 0)
    CheckMockKeyringItem(&mock_keyring_items[0], form_google_, "chrome");
  if (mock_keyring_items.size() > 1)
    CheckMockKeyringItem(&mock_keyring_items[1], form_google_, "chrome-42");

  // Check that we have set the persistent preference.
  EXPECT_TRUE(
      profile_.GetPrefs()->GetBoolean(prefs::kPasswordsUseLocalProfileId));

  // Normally we'd actually have a different profile. But in the test just reset
  // the profile's persistent pref; we pass in the local profile id anyway.
  profile_.GetPrefs()->SetBoolean(prefs::kPasswordsUseLocalProfileId, false);

  {
    NativeBackendGnome backend(24, profile_.GetPrefs());
    backend.Init();

    // Trigger the migration by looking something up.
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

    // There should be three passwords now.
    EXPECT_EQ(3u, mock_keyring_items.size());
    if (mock_keyring_items.size() > 0)
      CheckMockKeyringItem(&mock_keyring_items[0], form_google_, "chrome");
    if (mock_keyring_items.size() > 1)
      CheckMockKeyringItem(&mock_keyring_items[1], form_google_, "chrome-42");
    if (mock_keyring_items.size() > 2)
      CheckMockKeyringItem(&mock_keyring_items[2], form_google_, "chrome-24");

    // Now delete the password from this second profile.
    BrowserThread::PostTask(
        BrowserThread::DB, FROM_HERE,
        base::Bind(base::IgnoreResult(&NativeBackendGnome::RemoveLogin),
                   base::Unretained(&backend), form_google_));

    RunBothThreads();

    // The other two copies of the password in different profiles should remain.
    EXPECT_EQ(2u, mock_keyring_items.size());
    if (mock_keyring_items.size() > 0)
      CheckMockKeyringItem(&mock_keyring_items[0], form_google_, "chrome");
    if (mock_keyring_items.size() > 1)
      CheckMockKeyringItem(&mock_keyring_items[1], form_google_, "chrome-42");
  }
}
