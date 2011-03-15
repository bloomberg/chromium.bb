// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webdata/web_database.h"

#include <algorithm>
#include <limits>
#include <set>
#include <string>

#include "app/sql/statement.h"
#include "app/sql/transaction.h"
#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/tuple.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/autofill_country.h"
#include "chrome/browser/autofill/autofill_profile.h"
#include "chrome/browser/autofill/autofill_type.h"
#include "chrome/browser/autofill/credit_card.h"
#include "chrome/browser/diagnostics/sqlite_diagnostics.h"
#include "chrome/browser/history/history_database.h"
#include "chrome/browser/password_manager/encryptor.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/webdata/autofill_change.h"
#include "chrome/common/guid.h"
#include "content/common/notification_service.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/codec/png_codec.h"
#include "webkit/glue/form_field.h"
#include "webkit/glue/password_form.h"

// Encryptor is now in place for Windows and Mac.  The Linux implementation
// currently obfuscates only.  Mac Encryptor implementation can block the
// active thread while presenting UI to the user.  See |encryptor_mac.mm| for
// details.
// For details on the Linux work see:
//   http://crbug.com/25404

using webkit_glue::FormField;
using webkit_glue::PasswordForm;

// Constants for the |autofill_profile_phones| |type| column.
enum AutoFillPhoneType {
  kAutoFillPhoneNumber = 0,
  kAutoFillFaxNumber = 1
};

////////////////////////////////////////////////////////////////////////////////
//
// Schema
//   Note: The database stores time in seconds, UTC.
//
// keywords                 Most of the columns mirror that of a field in
//                          TemplateURL. See TemplateURL for more details.
//   id
//   short_name
//   keyword
//   favicon_url
//   url
//   show_in_default_list
//   safe_for_autoreplace
//   originating_url
//   date_created           This column was added after we allowed keywords.
//                          Keywords created before we started tracking
//                          creation date have a value of 0 for this.
//   usage_count
//   input_encodings        Semicolon separated list of supported input
//                          encodings, may be empty.
//   suggest_url
//   prepopulate_id         See TemplateURL::prepopulate_id.
//   autogenerate_keyword
//   logo_id                See TemplateURL::logo_id
//   created_by_policy      See TemplateURL::created_by_policy.  This was added
//                          in version 26.
//   instant_url            See TemplateURL::instant_url.  This was added
//                          in version 29.
//
// logins
//   origin_url
//   action_url
//   username_element
//   username_value
//   password_element
//   password_value
//   submit_element
//   signon_realm        The authority (scheme, host, port).
//   ssl_valid           SSL status of page containing the form at first
//                       impression.
//   preferred           MRU bit.
//   date_created        This column was added after logins support. "Legacy"
//                       entries have a value of 0.
//   blacklisted_by_user Tracks whether or not the user opted to 'never
//                       remember'
//                       passwords for this site.
//
// autofill
//   name                The name of the input as specified in the html.
//   value               The literal contents of the text field.
//   value_lower         The contents of the text field made lower_case.
//   pair_id             An ID number unique to the row in the table.
//   count               How many times the user has entered the string |value|
//                       in a field of name |name|.
//
// autofill_dates        This table associates a row to each separate time the
//                       user submits a form containing a certain name/value
//                       pair.  The |pair_id| should match the |pair_id| field
//                       in the appropriate row of the autofill table.
//   pair_id
//   date_created
//
// autofill_profiles    This table contains AutoFill profile data added by the
//                      user with the AutoFill dialog.  Most of the columns are
//                      standard entries in a contact information form.
//
//   guid               A guid string to uniquely identify the profile.
//                      Added in version 31.
//   company_name
//   address_line_1
//   address_line_2
//   city
//   state
//   zipcode
//   country            The country name.  Deprecated, should be removed once
//                      the stable channel reaches version 11.
//   country_code
//   date_modified      The date on which this profile was last modified.
//                      Added in version 30.
//
// autofill_profile_names
//                      This table contains the multi-valued name fields
//                      associated with a profile.
//
//   guid               The guid string that identifies the profile to which
//                      the name belongs.
//   first_name
//   middle_name
//   last_name
//
// autofill_profile_emails
//                      This table contains the multi-valued email fields
//                      associated with a profile.
//
//   guid               The guid string that identifies the profile to which
//                      the email belongs.
//   email
//
// autofill_profile_phones
//                      This table contains the multi-valued phone fields
//                      associated with a profile.
//
//   guid               The guid string that identifies the profile to which
//                      the phone or fax number belongs.
//   type               An integer constant designating either phone or fax type
//                      of the number.
//   number
//
// credit_cards         This table contains credit card data added by the user
//                      with the AutoFill dialog.  Most of the columns are
//                      standard entries in a credit card form.
//
//   guid               A guid string to uniquely identify the profile.
//                      Added in version 31.
//   name_on_card
//   expiration_month
//   expiration_year
//   card_number_encrypted Stores encrypted credit card number.
//   date_modified      The date on which this entry was last modified.
//                      Added in version 30.
//
// web_app_icons
//   url         URL of the web app.
//   width       Width of the image.
//   height      Height of the image.
//   image       PNG encoded image data.
//
// web_apps
//   url                 URL of the web app.
//   has_all_images      Do we have all the images?
//
////////////////////////////////////////////////////////////////////////////////

using base::Time;

namespace {

typedef std::vector<Tuple3<int64, string16, string16> > AutofillElementList;

// Current version number.  Note: when changing the current version number,
// corresponding changes must happen in the unit tests, and new migration test
// added.  See |WebDatabaseMigrationTest::kCurrentTestedVersionNumber|.
const int kCurrentVersionNumber = 35;
const int kCompatibleVersionNumber = 35;

// ID of the url column in keywords.
const int kUrlIdPosition = 16;

// Keys used in the meta table.
const char* kDefaultSearchProviderKey = "Default Search Provider ID";
const char* kBuiltinKeywordVersion = "Builtin Keyword Version";

// The maximum length allowed for form data.
const size_t kMaxDataLength = 1024;

void BindURLToStatement(const TemplateURL& url, sql::Statement* s) {
  s->BindString(0, UTF16ToUTF8(url.short_name()));
  s->BindString(1, UTF16ToUTF8(url.keyword()));
  GURL favicon_url = url.GetFavIconURL();
  if (!favicon_url.is_valid()) {
    s->BindString(2, std::string());
  } else {
    s->BindString(2, history::HistoryDatabase::GURLToDatabaseURL(
                       url.GetFavIconURL()));
  }
  s->BindString(3, url.url() ? url.url()->url() : std::string());
  s->BindInt(4, url.safe_for_autoreplace() ? 1 : 0);
  if (!url.originating_url().is_valid()) {
    s->BindString(5, std::string());
  } else {
    s->BindString(5, history::HistoryDatabase::GURLToDatabaseURL(
        url.originating_url()));
  }
  s->BindInt64(6, url.date_created().ToTimeT());
  s->BindInt(7, url.usage_count());
  s->BindString(8, JoinString(url.input_encodings(), ';'));
  s->BindInt(9, url.show_in_default_list() ? 1 : 0);
  s->BindString(10, url.suggestions_url() ? url.suggestions_url()->url() :
                std::string());
  s->BindInt(11, url.prepopulate_id());
  s->BindInt(12, url.autogenerate_keyword() ? 1 : 0);
  s->BindInt(13, url.logo_id());
  s->BindBool(14, url.created_by_policy());
  s->BindString(15, url.instant_url() ? url.instant_url()->url() :
                std::string());
}

void InitPasswordFormFromStatement(PasswordForm* form, sql::Statement* s) {
  std::string tmp;
  string16 decrypted_password;
  tmp = s->ColumnString(0);
  form->origin = GURL(tmp);
  tmp = s->ColumnString(1);
  form->action = GURL(tmp);
  form->username_element = s->ColumnString16(2);
  form->username_value = s->ColumnString16(3);
  form->password_element = s->ColumnString16(4);

  int encrypted_password_len = s->ColumnByteLength(5);
  std::string encrypted_password;
  if (encrypted_password_len) {
    encrypted_password.resize(encrypted_password_len);
    memcpy(&encrypted_password[0], s->ColumnBlob(5), encrypted_password_len);
    Encryptor::DecryptString16(encrypted_password, &decrypted_password);
  }

  form->password_value = decrypted_password;
  form->submit_element = s->ColumnString16(6);
  tmp = s->ColumnString(7);
  form->signon_realm = tmp;
  form->ssl_valid = (s->ColumnInt(8) > 0);
  form->preferred = (s->ColumnInt(9) > 0);
  form->date_created = Time::FromTimeT(s->ColumnInt64(10));
  form->blacklisted_by_user = (s->ColumnInt(11) > 0);
  int scheme_int = s->ColumnInt(12);
  DCHECK((scheme_int >= 0) && (scheme_int <= PasswordForm::SCHEME_OTHER));
  form->scheme = static_cast<PasswordForm::Scheme>(scheme_int);
}

// TODO(jhawkins): This is a temporary stop-gap measure designed to prevent
// a malicious site from DOS'ing the browser with extremely large profile
// data.  The correct solution is to parse this data asynchronously.
// See http://crbug.com/49332.
string16 LimitDataSize(const string16& data) {
  if (data.size() > kMaxDataLength)
    return data.substr(0, kMaxDataLength);

  return data;
}

void BindAutofillProfileToStatement(const AutofillProfile& profile,
                                    sql::Statement* s) {
  DCHECK(guid::IsValidGUID(profile.guid()));
  s->BindString(0, profile.guid());

  string16 text = profile.GetFieldText(AutofillType(COMPANY_NAME));
  s->BindString16(1, LimitDataSize(text));
  text = profile.GetFieldText(AutofillType(ADDRESS_HOME_LINE1));
  s->BindString16(2, LimitDataSize(text));
  text = profile.GetFieldText(AutofillType(ADDRESS_HOME_LINE2));
  s->BindString16(3, LimitDataSize(text));
  text = profile.GetFieldText(AutofillType(ADDRESS_HOME_CITY));
  s->BindString16(4, LimitDataSize(text));
  text = profile.GetFieldText(AutofillType(ADDRESS_HOME_STATE));
  s->BindString16(5, LimitDataSize(text));
  text = profile.GetFieldText(AutofillType(ADDRESS_HOME_ZIP));
  s->BindString16(6, LimitDataSize(text));
  text = profile.GetFieldText(AutofillType(ADDRESS_HOME_COUNTRY));
  s->BindString16(7, LimitDataSize(text));
  std::string country_code = profile.CountryCode();
  s->BindString(8, country_code);
  s->BindInt64(9, Time::Now().ToTimeT());
}

AutofillProfile* AutofillProfileFromStatement(const sql::Statement& s) {
  AutofillProfile* profile = new AutofillProfile;
  profile->set_guid(s.ColumnString(0));
  DCHECK(guid::IsValidGUID(profile->guid()));

  profile->SetInfo(AutofillType(COMPANY_NAME), s.ColumnString16(1));
  profile->SetInfo(AutofillType(ADDRESS_HOME_LINE1), s.ColumnString16(2));
  profile->SetInfo(AutofillType(ADDRESS_HOME_LINE2), s.ColumnString16(3));
  profile->SetInfo(AutofillType(ADDRESS_HOME_CITY), s.ColumnString16(4));
  profile->SetInfo(AutofillType(ADDRESS_HOME_STATE), s.ColumnString16(5));
  profile->SetInfo(AutofillType(ADDRESS_HOME_ZIP), s.ColumnString16(6));
  // Intentionally skip column 7, which stores the localized country name.
  profile->SetCountryCode(s.ColumnString(8));
  // Intentionally skip column 9, which stores the profile's modification date.

  return profile;
}

void AddAutofillProfileNameFromStatement(const sql::Statement& s,
                                      AutofillProfile* profile) {
  DCHECK_EQ(profile->guid(), s.ColumnString(0));
  DCHECK(guid::IsValidGUID(profile->guid()));

  profile->SetInfo(AutofillType(NAME_FIRST), s.ColumnString16(1));
  profile->SetInfo(AutofillType(NAME_MIDDLE), s.ColumnString16(2));
  profile->SetInfo(AutofillType(NAME_LAST), s.ColumnString16(3));
}

void AddAutofillProfileEmailFromStatement(const sql::Statement& s,
                                       AutofillProfile* profile) {
  DCHECK_EQ(profile->guid(), s.ColumnString(0));
  DCHECK(guid::IsValidGUID(profile->guid()));

  profile->SetInfo(AutofillType(EMAIL_ADDRESS), s.ColumnString16(1));
}

void AddAutofillProfilePhoneFromStatement(const sql::Statement& s,
                                       AutofillProfile* profile) {
  DCHECK_EQ(profile->guid(), s.ColumnString(0));
  DCHECK(guid::IsValidGUID(profile->guid()));
  DCHECK_EQ(kAutoFillPhoneNumber, s.ColumnInt(1));
  profile->SetInfo(AutofillType(PHONE_HOME_WHOLE_NUMBER), s.ColumnString16(2));
}

void AddAutofillProfileFaxFromStatement(const sql::Statement& s,
                                     AutofillProfile* profile) {
  DCHECK_EQ(profile->guid(), s.ColumnString(0));
  DCHECK(guid::IsValidGUID(profile->guid()));
  DCHECK_EQ(kAutoFillFaxNumber, s.ColumnInt(1));
  profile->SetInfo(AutofillType(PHONE_FAX_WHOLE_NUMBER), s.ColumnString16(2));
}

void BindCreditCardToStatement(const CreditCard& credit_card,
                               sql::Statement* s) {
  DCHECK(guid::IsValidGUID(credit_card.guid()));
  s->BindString(0, credit_card.guid());

  string16 text = credit_card.GetFieldText(AutofillType(CREDIT_CARD_NAME));
  s->BindString16(1, LimitDataSize(text));
  text = credit_card.GetFieldText(AutofillType(CREDIT_CARD_EXP_MONTH));
  s->BindString16(2, LimitDataSize(text));
  text = credit_card.GetFieldText(AutofillType(CREDIT_CARD_EXP_4_DIGIT_YEAR));
  s->BindString16(3, LimitDataSize(text));
  text = credit_card.GetFieldText(AutofillType(CREDIT_CARD_NUMBER));
  std::string encrypted_data;
  Encryptor::EncryptString16(text, &encrypted_data);
  s->BindBlob(4, encrypted_data.data(),
              static_cast<int>(encrypted_data.length()));
  s->BindInt64(5, Time::Now().ToTimeT());
}

CreditCard* CreditCardFromStatement(const sql::Statement& s) {
  CreditCard* credit_card = new CreditCard;

  credit_card->set_guid(s.ColumnString(0));
  DCHECK(guid::IsValidGUID(credit_card->guid()));

  credit_card->SetInfo(AutofillType(CREDIT_CARD_NAME), s.ColumnString16(1));
  credit_card->SetInfo(AutofillType(CREDIT_CARD_EXP_MONTH),
                       s.ColumnString16(2));
  credit_card->SetInfo(AutofillType(CREDIT_CARD_EXP_4_DIGIT_YEAR),
                       s.ColumnString16(3));
  int encrypted_number_len = s.ColumnByteLength(4);
  string16 credit_card_number;
  if (encrypted_number_len) {
    std::string encrypted_number;
    encrypted_number.resize(encrypted_number_len);
    memcpy(&encrypted_number[0], s.ColumnBlob(4), encrypted_number_len);
    Encryptor::DecryptString16(encrypted_number, &credit_card_number);
  }
  credit_card->SetInfo(AutofillType(CREDIT_CARD_NUMBER), credit_card_number);
  // Intentionally skip column 5, which stores the modification date.

  return credit_card;
}

bool AutofillProfileHasName(const AutofillProfile& profile) {
  return !profile.GetFieldText(AutofillType(NAME_FIRST)).empty() ||
         !profile.GetFieldText(AutofillType(NAME_MIDDLE)).empty() ||
         !profile.GetFieldText(AutofillType(NAME_MIDDLE)).empty();
}

bool AddAutofillProfileName(const std::string& guid,
                            const AutofillProfile& profile,
                            sql::Connection* db) {
  if (!AutofillProfileHasName(profile))
    return true;

  // Check for duplicate.
  sql::Statement s_find(db->GetUniqueStatement(
    "SELECT guid, first_name, middle_name, last_name "
    "FROM autofill_profile_names "
    "WHERE guid=? AND first_name=? AND middle_name=? AND last_name=?"));
  if (!s_find) {
    NOTREACHED();
    return false;
  }
  s_find.BindString(0, guid);
  s_find.BindString16(1, profile.GetFieldText(AutofillType(NAME_FIRST)));
  s_find.BindString16(2, profile.GetFieldText(AutofillType(NAME_MIDDLE)));
  s_find.BindString16(3, profile.GetFieldText(AutofillType(NAME_LAST)));

  if (!s_find.Step()) {
    // Add the new name.
    sql::Statement s(db->GetUniqueStatement(
      "INSERT INTO autofill_profile_names"
      " (guid, first_name, middle_name, last_name) "
      "VALUES (?,?,?,?)"));
    if (!s) {
      NOTREACHED();
      return false;
    }
    s.BindString(0, guid);
    s.BindString16(1, profile.GetFieldText(AutofillType(NAME_FIRST)));
    s.BindString16(2, profile.GetFieldText(AutofillType(NAME_MIDDLE)));
    s.BindString16(3, profile.GetFieldText(AutofillType(NAME_LAST)));

    if (!s.Run()) {
      NOTREACHED();
      return false;
    }
  }
  return true;
}

bool AddAutofillProfileEmail(const std::string& guid,
                             const AutofillProfile& profile,
                             sql::Connection* db) {
  if (profile.GetFieldText(AutofillType(EMAIL_ADDRESS)).empty())
    return true;

  // Check for duplicate.
  sql::Statement s_find(db->GetUniqueStatement(
    "SELECT guid, email "
    "FROM autofill_profile_emails "
    "WHERE guid=? AND email=?"));
  if (!s_find) {
    NOTREACHED();
    return false;
  }
  s_find.BindString(0, guid);
  s_find.BindString16(1, profile.GetFieldText(AutofillType(EMAIL_ADDRESS)));

  if (!s_find.Step()) {
    sql::Statement s(db->GetUniqueStatement(
      "INSERT INTO autofill_profile_emails"
      " (guid, email) "
      "VALUES (?,?)"));
    if (!s) {
      NOTREACHED();
      return false;
    }
    s.BindString(0, guid);
    s.BindString16(1, profile.GetFieldText(AutofillType(EMAIL_ADDRESS)));

    if (!s.Run()) {
      NOTREACHED();
      return false;
    }
  }
  return true;
}

bool AddAutofillProfilePhone(const std::string& guid,
                             const AutofillProfile& profile,
                             AutoFillPhoneType phone_type,
                             sql::Connection* db) {
  AutofillFieldType field_type;
  if (phone_type == kAutoFillPhoneNumber) {
    field_type = PHONE_HOME_WHOLE_NUMBER;
  } else if (phone_type == kAutoFillFaxNumber) {
    field_type = PHONE_FAX_WHOLE_NUMBER;
  } else {
    NOTREACHED();
    return false;
  }

  if (profile.GetFieldText(AutofillType(field_type)).empty())
    return true;

  // Check for duplicate.
  sql::Statement s_find(db->GetUniqueStatement(
    "SELECT guid, type, number "
    "FROM autofill_profile_phones "
    "WHERE guid=? AND type=? AND number=?"));
  if (!s_find) {
    NOTREACHED();
    return false;
  }
  s_find.BindString(0, guid);
  s_find.BindInt(1, phone_type);
  s_find.BindString16(
      2, profile.GetFieldText(AutofillType(field_type)));

  if (!s_find.Step()) {
    sql::Statement s(db->GetUniqueStatement(
      "INSERT INTO autofill_profile_phones"
      " (guid, type, number) "
      "VALUES (?,?,?)"));
    if (!s) {
      NOTREACHED();
      return sql::INIT_FAILURE;
    }
    s.BindString(0, guid);
    s.BindInt(1, phone_type);
    s.BindString16(
        2, profile.GetFieldText(AutofillType(field_type)));

    if (!s.Run()) {
      NOTREACHED();
      return false;
    }
  }
  return true;
}

bool AddAutofillProfilePieces(const std::string& guid,
                              const AutofillProfile& profile,
                              sql::Connection* db) {
  if (!AddAutofillProfileName(guid, profile, db))
    return false;

  if (!AddAutofillProfileEmail(guid, profile, db))
    return false;

  if (!AddAutofillProfilePhone(guid, profile, kAutoFillPhoneNumber, db))
    return false;

  if (!AddAutofillProfilePhone(guid, profile, kAutoFillFaxNumber, db))
    return false;

  return true;
}

bool RemoveAutofillProfilePieces(const std::string& guid, sql::Connection* db) {
  sql::Statement s1(db->GetUniqueStatement(
      "DELETE FROM autofill_profile_names WHERE guid = ?"));
  if (!s1) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  s1.BindString(0, guid);
  if (!s1.Run())
    return false;

  sql::Statement s2(db->GetUniqueStatement(
      "DELETE FROM autofill_profile_emails WHERE guid = ?"));
  if (!s2) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  s2.BindString(0, guid);
  if (!s2.Run())
    return false;

  sql::Statement s3(db->GetUniqueStatement(
      "DELETE FROM autofill_profile_phones WHERE guid = ?"));
  if (!s3) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  s3.BindString(0, guid);
  return s3.Run();
}

}  // namespace

WebDatabase::WebDatabase() {
}

WebDatabase::~WebDatabase() {
}

void WebDatabase::BeginTransaction() {
  db_.BeginTransaction();
}

void WebDatabase::CommitTransaction() {
  db_.CommitTransaction();
}

sql::InitStatus WebDatabase::Init(const FilePath& db_name) {
  // When running in unit tests, there is already a NotificationService object.
  // Since only one can exist at a time per thread, check first.
  if (!NotificationService::current())
    notification_service_.reset(new NotificationService);

  // Set the exceptional sqlite error handler.
  db_.set_error_delegate(GetErrorHandlerForWebDb());

  // We don't store that much data in the tables so use a small page size.
  // This provides a large benefit for empty tables (which is very likely with
  // the tables we create).
  db_.set_page_size(2048);

  // We shouldn't have much data and what access we currently have is quite
  // infrequent. So we go with a small cache size.
  db_.set_cache_size(32);

  // Run the database in exclusive mode. Nobody else should be accessing the
  // database while we're running, and this will give somewhat improved perf.
  db_.set_exclusive_locking();

  if (!db_.Open(db_name))
    return sql::INIT_FAILURE;

  // Initialize various tables
  sql::Transaction transaction(&db_);
  if (!transaction.Begin())
    return sql::INIT_FAILURE;

  // Version check.
  if (!meta_table_.Init(&db_, kCurrentVersionNumber, kCompatibleVersionNumber))
    return sql::INIT_FAILURE;
  if (meta_table_.GetCompatibleVersionNumber() > kCurrentVersionNumber) {
    LOG(WARNING) << "Web database is too new.";
    return sql::INIT_TOO_NEW;
  }

  // Initialize the tables.
  if (!InitKeywordsTable() || !InitLoginsTable() || !InitWebAppIconsTable() ||
      !InitWebAppsTable() || !InitAutofillTable() ||
      !InitAutofillDatesTable() || !InitAutofillProfilesTable() ||
      !InitAutofillProfileNamesTable() || !InitAutofillProfileEmailsTable() ||
      !InitAutofillProfilePhonesTable() || !InitCreditCardsTable() ||
      !InitTokenServiceTable()) {
    LOG(WARNING) << "Unable to initialize the web database.";
    return sql::INIT_FAILURE;
  }

  // If the file on disk is an older database version, bring it up to date.
  // If the migration fails we return an error to caller and do not commit
  // the migration.
  sql::InitStatus migration_status = MigrateOldVersionsAsNeeded();
  if (migration_status != sql::INIT_OK)
    return migration_status;

  return transaction.Commit() ? sql::INIT_OK : sql::INIT_FAILURE;
}

bool WebDatabase::SetWebAppImage(const GURL& url, const SkBitmap& image) {
  // Don't bother with a cached statement since this will be a relatively
  // infrequent operation.
  sql::Statement s(db_.GetUniqueStatement(
      "INSERT OR REPLACE INTO web_app_icons "
      "(url, width, height, image) VALUES (?, ?, ?, ?)"));
  if (!s)
    return false;

  std::vector<unsigned char> image_data;
  gfx::PNGCodec::EncodeBGRASkBitmap(image, false, &image_data);

  s.BindString(0, history::HistoryDatabase::GURLToDatabaseURL(url));
  s.BindInt(1, image.width());
  s.BindInt(2, image.height());
  s.BindBlob(3, &image_data.front(), static_cast<int>(image_data.size()));
  return s.Run();
}

bool WebDatabase::GetWebAppImages(const GURL& url,
                                  std::vector<SkBitmap>* images) {
  sql::Statement s(db_.GetUniqueStatement(
      "SELECT image FROM web_app_icons WHERE url=?"));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }
  s.BindString(0, history::HistoryDatabase::GURLToDatabaseURL(url));
  while (s.Step()) {
    SkBitmap image;
    int col_bytes = s.ColumnByteLength(0);
    if (col_bytes > 0) {
      if (gfx::PNGCodec::Decode(
              reinterpret_cast<const unsigned char*>(s.ColumnBlob(0)),
              col_bytes, &image)) {
        images->push_back(image);
      } else {
        // Should only have valid image data in the db.
        NOTREACHED();
      }
    }
  }
  return true;
}

bool WebDatabase::SetWebAppHasAllImages(const GURL& url,
                                        bool has_all_images) {
  sql::Statement s(db_.GetUniqueStatement(
      "INSERT OR REPLACE INTO web_apps (url, has_all_images) VALUES (?, ?)"));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }
  s.BindString(0, history::HistoryDatabase::GURLToDatabaseURL(url));
  s.BindInt(1, has_all_images ? 1 : 0);
  return s.Run();
}

bool WebDatabase::GetWebAppHasAllImages(const GURL& url) {
  sql::Statement s(db_.GetUniqueStatement(
      "SELECT has_all_images FROM web_apps WHERE url=?"));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }
  s.BindString(0, history::HistoryDatabase::GURLToDatabaseURL(url));
  return (s.Step() && s.ColumnInt(0) == 1);
}

bool WebDatabase::RemoveWebApp(const GURL& url) {
  sql::Statement delete_s(db_.GetUniqueStatement(
      "DELETE FROM web_app_icons WHERE url = ?"));
  if (!delete_s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }
  delete_s.BindString(0, history::HistoryDatabase::GURLToDatabaseURL(url));
  if (!delete_s.Run())
    return false;

  sql::Statement delete_s2(db_.GetUniqueStatement(
      "DELETE FROM web_apps WHERE url = ?"));
  if (!delete_s2) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }
  delete_s2.BindString(0, history::HistoryDatabase::GURLToDatabaseURL(url));
  return delete_s2.Run();
}

bool WebDatabase::RemoveAllTokens() {
  sql::Statement s(db_.GetUniqueStatement(
      "DELETE FROM token_service"));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  return s.Run();
}

bool WebDatabase::SetTokenForService(const std::string& service,
                                     const std::string& token) {
  // Don't bother with a cached statement since this will be a relatively
  // infrequent operation.
  sql::Statement s(db_.GetUniqueStatement(
      "INSERT OR REPLACE INTO token_service "
      "(service, encrypted_token) VALUES (?, ?)"));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  std::string encrypted_token;

  bool encrypted = Encryptor::EncryptString(token, &encrypted_token);
  if (!encrypted) {
    return false;
  }

  s.BindString(0, service);
  s.BindBlob(1, encrypted_token.data(),
             static_cast<int>(encrypted_token.length()));
  return s.Run();
}

bool WebDatabase::GetAllTokens(std::map<std::string, std::string>* tokens) {
  sql::Statement s(db_.GetUniqueStatement(
      "SELECT service, encrypted_token FROM token_service"));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  while (s.Step()) {
    std::string encrypted_token;
    std::string decrypted_token;
    std::string service;
    service = s.ColumnString(0);
    bool entry_ok = !service.empty() &&
                    s.ColumnBlobAsString(1, &encrypted_token);
    if (entry_ok) {
      Encryptor::DecryptString(encrypted_token, &decrypted_token);
      (*tokens)[service] = decrypted_token;
    } else {
      NOTREACHED();
      return false;
    }
  }
  return true;
}

bool WebDatabase::InitKeywordsTable() {
  if (!db_.DoesTableExist("keywords")) {
    if (!db_.Execute("CREATE TABLE keywords ("
                     "id INTEGER PRIMARY KEY,"
                     "short_name VARCHAR NOT NULL,"
                     "keyword VARCHAR NOT NULL,"
                     "favicon_url VARCHAR NOT NULL,"
                     "url VARCHAR NOT NULL,"
                     "show_in_default_list INTEGER,"
                     "safe_for_autoreplace INTEGER,"
                     "originating_url VARCHAR,"
                     "date_created INTEGER DEFAULT 0,"
                     "usage_count INTEGER DEFAULT 0,"
                     "input_encodings VARCHAR,"
                     "suggest_url VARCHAR,"
                     "prepopulate_id INTEGER DEFAULT 0,"
                     "autogenerate_keyword INTEGER DEFAULT 0,"
                     "logo_id INTEGER DEFAULT 0,"
                     "created_by_policy INTEGER DEFAULT 0,"
                     "instant_url VARCHAR)")) {
      NOTREACHED();
      return false;
    }
  }
  return true;
}

bool WebDatabase::InitLoginsTable() {
  if (!db_.DoesTableExist("logins")) {
    if (!db_.Execute("CREATE TABLE logins ("
                     "origin_url VARCHAR NOT NULL, "
                     "action_url VARCHAR, "
                     "username_element VARCHAR, "
                     "username_value VARCHAR, "
                     "password_element VARCHAR, "
                     "password_value BLOB, "
                     "submit_element VARCHAR, "
                     "signon_realm VARCHAR NOT NULL,"
                     "ssl_valid INTEGER NOT NULL,"
                     "preferred INTEGER NOT NULL,"
                     "date_created INTEGER NOT NULL,"
                     "blacklisted_by_user INTEGER NOT NULL,"
                     "scheme INTEGER NOT NULL,"
                     "UNIQUE "
                     "(origin_url, username_element, "
                     "username_value, password_element, "
                     "submit_element, signon_realm))")) {
      NOTREACHED();
      return false;
    }
    if (!db_.Execute("CREATE INDEX logins_signon ON logins (signon_realm)")) {
      NOTREACHED();
      return false;
    }
  }

#if defined(OS_WIN)
  if (!db_.DoesTableExist("ie7_logins")) {
    if (!db_.Execute("CREATE TABLE ie7_logins ("
                     "url_hash VARCHAR NOT NULL, "
                     "password_value BLOB, "
                     "date_created INTEGER NOT NULL,"
                     "UNIQUE "
                     "(url_hash))")) {
      NOTREACHED();
      return false;
    }
    if (!db_.Execute("CREATE INDEX ie7_logins_hash ON "
                     "ie7_logins (url_hash)")) {
      NOTREACHED();
      return false;
    }
  }
#endif

  return true;
}

bool WebDatabase::InitAutofillTable() {
  if (!db_.DoesTableExist("autofill")) {
    if (!db_.Execute("CREATE TABLE autofill ("
                     "name VARCHAR, "
                     "value VARCHAR, "
                     "value_lower VARCHAR, "
                     "pair_id INTEGER PRIMARY KEY, "
                     "count INTEGER DEFAULT 1)")) {
      NOTREACHED();
      return false;
    }
    if (!db_.Execute("CREATE INDEX autofill_name ON autofill (name)")) {
       NOTREACHED();
       return false;
    }
    if (!db_.Execute("CREATE INDEX autofill_name_value_lower ON "
                     "autofill (name, value_lower)")) {
       NOTREACHED();
       return false;
    }
  }
  return true;
}

bool WebDatabase::InitAutofillDatesTable() {
  if (!db_.DoesTableExist("autofill_dates")) {
    if (!db_.Execute("CREATE TABLE autofill_dates ( "
                     "pair_id INTEGER DEFAULT 0, "
                     "date_created INTEGER DEFAULT 0)")) {
      NOTREACHED();
      return false;
    }
    if (!db_.Execute("CREATE INDEX autofill_dates_pair_id ON "
                     "autofill_dates (pair_id)")) {
      NOTREACHED();
      return false;
    }
  }
  return true;
}

bool WebDatabase::InitAutofillProfilesTable() {
  if (!db_.DoesTableExist("autofill_profiles")) {
    if (!db_.Execute("CREATE TABLE autofill_profiles ( "
                     "guid VARCHAR PRIMARY KEY, "
                     "company_name VARCHAR, "
                     "address_line_1 VARCHAR, "
                     "address_line_2 VARCHAR, "
                     "city VARCHAR, "
                     "state VARCHAR, "
                     "zipcode VARCHAR, "
                     "country VARCHAR, "
                     "country_code VARCHAR, "
                     "date_modified INTEGER NOT NULL DEFAULT 0)")) {
      NOTREACHED();
      return false;
    }
  }
  return true;
}

bool WebDatabase::InitAutofillProfileNamesTable() {
  if (!db_.DoesTableExist("autofill_profile_names")) {
    if (!db_.Execute("CREATE TABLE autofill_profile_names ( "
                     "guid VARCHAR, "
                     "first_name VARCHAR, "
                     "middle_name VARCHAR, "
                     "last_name VARCHAR)")) {
      NOTREACHED();
      return false;
    }
  }
  return true;
}

bool WebDatabase::InitAutofillProfileEmailsTable() {
  if (!db_.DoesTableExist("autofill_profile_emails")) {
    if (!db_.Execute("CREATE TABLE autofill_profile_emails ( "
                     "guid VARCHAR, "
                     "email VARCHAR)")) {
      NOTREACHED();
      return false;
    }
  }
  return true;
}

bool WebDatabase::InitAutofillProfilePhonesTable() {
  if (!db_.DoesTableExist("autofill_profile_phones")) {
    if (!db_.Execute("CREATE TABLE autofill_profile_phones ( "
                     "guid VARCHAR, "
                     "type INTEGER DEFAULT 0, "
                     "number VARCHAR)")) {
      NOTREACHED();
      return false;
    }
  }
  return true;
}

bool WebDatabase::InitCreditCardsTable() {
  if (!db_.DoesTableExist("credit_cards")) {
    if (!db_.Execute("CREATE TABLE credit_cards ( "
                     "guid VARCHAR PRIMARY KEY, "
                     "name_on_card VARCHAR, "
                     "expiration_month INTEGER, "
                     "expiration_year INTEGER, "
                     "card_number_encrypted BLOB, "
                     "date_modified INTEGER NOT NULL DEFAULT 0)")) {
      NOTREACHED();
      return false;
    }
  }

  return true;
}

bool WebDatabase::InitWebAppIconsTable() {
  if (!db_.DoesTableExist("web_app_icons")) {
    if (!db_.Execute("CREATE TABLE web_app_icons ("
                     "url LONGVARCHAR,"
                     "width int,"
                     "height int,"
                     "image BLOB, UNIQUE (url, width, height))")) {
      NOTREACHED();
      return false;
    }
  }
  return true;
}

bool WebDatabase::InitWebAppsTable() {
  if (!db_.DoesTableExist("web_apps")) {
    if (!db_.Execute("CREATE TABLE web_apps ("
                     "url LONGVARCHAR UNIQUE,"
                     "has_all_images INTEGER NOT NULL)")) {
      NOTREACHED();
      return false;
    }
    if (!db_.Execute("CREATE INDEX web_apps_url_index ON web_apps (url)")) {
      NOTREACHED();
      return false;
    }
  }
  return true;
}

bool WebDatabase::InitTokenServiceTable() {
  if (!db_.DoesTableExist("token_service")) {
    if (!db_.Execute("CREATE TABLE token_service ("
                     "service VARCHAR PRIMARY KEY NOT NULL,"
                     "encrypted_token BLOB)")) {
      NOTREACHED();
      return false;
    }
  }
  return true;
}

bool WebDatabase::AddKeyword(const TemplateURL& url) {
  DCHECK(url.id());
  // Be sure to change kUrlIdPosition if you add columns
  sql::Statement s(db_.GetCachedStatement(SQL_FROM_HERE,
      "INSERT INTO keywords "
      "(short_name, keyword, favicon_url, url, safe_for_autoreplace, "
      "originating_url, date_created, usage_count, input_encodings, "
      "show_in_default_list, suggest_url, prepopulate_id, "
      "autogenerate_keyword, logo_id, created_by_policy, instant_url, "
      "id) VALUES "
      "(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)"));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }
  BindURLToStatement(url, &s);
  s.BindInt64(kUrlIdPosition, url.id());
  if (!s.Run()) {
    NOTREACHED();
    return false;
  }
  return true;
}

bool WebDatabase::RemoveKeyword(TemplateURLID id) {
  DCHECK(id);
  sql::Statement s(db_.GetUniqueStatement("DELETE FROM keywords WHERE id = ?"));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }
  s.BindInt64(0, id);
  return s.Run();
}

bool WebDatabase::GetKeywords(std::vector<TemplateURL*>* urls) {
  sql::Statement s(db_.GetUniqueStatement(
      "SELECT id, short_name, keyword, favicon_url, url, "
      "safe_for_autoreplace, originating_url, date_created, "
      "usage_count, input_encodings, show_in_default_list, "
      "suggest_url, prepopulate_id, autogenerate_keyword, logo_id, "
      "created_by_policy, instant_url "
      "FROM keywords ORDER BY id ASC"));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }
  while (s.Step()) {
    TemplateURL* template_url = new TemplateURL();
    template_url->set_id(s.ColumnInt64(0));

    std::string tmp;
    tmp = s.ColumnString(1);
    DCHECK(!tmp.empty());
    template_url->set_short_name(UTF8ToUTF16(tmp));

    template_url->set_keyword(UTF8ToUTF16(s.ColumnString(2)));

    tmp = s.ColumnString(3);
    if (!tmp.empty())
      template_url->SetFaviconURL(GURL(tmp));

    template_url->SetURL(s.ColumnString(4), 0, 0);

    template_url->set_safe_for_autoreplace(s.ColumnInt(5) == 1);

    tmp = s.ColumnString(6);
    if (!tmp.empty())
      template_url->set_originating_url(GURL(tmp));

    template_url->set_date_created(Time::FromTimeT(s.ColumnInt64(7)));

    template_url->set_usage_count(s.ColumnInt(8));

    std::vector<std::string> encodings;
    base::SplitString(s.ColumnString(9), ';', &encodings);
    template_url->set_input_encodings(encodings);

    template_url->set_show_in_default_list(s.ColumnInt(10) == 1);

    template_url->SetSuggestionsURL(s.ColumnString(11), 0, 0);

    template_url->set_prepopulate_id(s.ColumnInt(12));

    template_url->set_autogenerate_keyword(s.ColumnInt(13) == 1);

    template_url->set_logo_id(s.ColumnInt(14));

    template_url->set_created_by_policy(s.ColumnBool(15));

    template_url->SetInstantURL(s.ColumnString(16), 0, 0);

    urls->push_back(template_url);
  }
  return s.Succeeded();
}

bool WebDatabase::UpdateKeyword(const TemplateURL& url) {
  DCHECK(url.id());
  // Be sure to change kUrlIdPosition if you add columns
  sql::Statement s(db_.GetUniqueStatement(
      "UPDATE keywords "
      "SET short_name=?, keyword=?, favicon_url=?, url=?, "
      "safe_for_autoreplace=?, originating_url=?, date_created=?, "
      "usage_count=?, input_encodings=?, show_in_default_list=?, "
      "suggest_url=?, prepopulate_id=?, autogenerate_keyword=?, "
      "logo_id=?, created_by_policy=?, instant_url=? WHERE id=?"));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }
  BindURLToStatement(url, &s);
  s.BindInt64(kUrlIdPosition, url.id());
  return s.Run();
}

bool WebDatabase::SetDefaultSearchProviderID(int64 id) {
  return meta_table_.SetValue(kDefaultSearchProviderKey, id);
}

int64 WebDatabase::GetDefaulSearchProviderID() {
  int64 value = 0;
  meta_table_.GetValue(kDefaultSearchProviderKey, &value);
  return value;
}

bool WebDatabase::SetBuitinKeywordVersion(int version) {
  return meta_table_.SetValue(kBuiltinKeywordVersion, version);
}

int WebDatabase::GetBuitinKeywordVersion() {
  int version = 0;
  meta_table_.GetValue(kBuiltinKeywordVersion, &version);
  return version;
}

bool WebDatabase::AddLogin(const PasswordForm& form) {
  sql::Statement s(db_.GetUniqueStatement(
      "INSERT OR REPLACE INTO logins "
      "(origin_url, action_url, username_element, username_value, "
      " password_element, password_value, submit_element, "
      " signon_realm, ssl_valid, preferred, date_created, "
      " blacklisted_by_user, scheme) "
      "VALUES "
      "(?,?,?,?,?,?,?,?,?,?,?,?,?)"));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  std::string encrypted_password;
  s.BindString(0, form.origin.spec());
  s.BindString(1, form.action.spec());
  s.BindString16(2, form.username_element);
  s.BindString16(3, form.username_value);
  s.BindString16(4, form.password_element);
  Encryptor::EncryptString16(form.password_value, &encrypted_password);
  s.BindBlob(5, encrypted_password.data(),
             static_cast<int>(encrypted_password.length()));
  s.BindString16(6, form.submit_element);
  s.BindString(7, form.signon_realm);
  s.BindInt(8, form.ssl_valid);
  s.BindInt(9, form.preferred);
  s.BindInt64(10, form.date_created.ToTimeT());
  s.BindInt(11, form.blacklisted_by_user);
  s.BindInt(12, form.scheme);
  if (!s.Run()) {
    NOTREACHED();
    return false;
  }
  return true;
}

bool WebDatabase::UpdateLogin(const PasswordForm& form) {
  sql::Statement s(db_.GetUniqueStatement(
      "UPDATE logins SET "
      "action_url = ?, "
      "password_value = ?, "
      "ssl_valid = ?, "
      "preferred = ? "
      "WHERE origin_url = ? AND "
      "username_element = ? AND "
      "username_value = ? AND "
      "password_element = ? AND "
      "signon_realm = ?"));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  s.BindString(0, form.action.spec());
  std::string encrypted_password;
  Encryptor::EncryptString16(form.password_value, &encrypted_password);
  s.BindBlob(1, encrypted_password.data(),
             static_cast<int>(encrypted_password.length()));
  s.BindInt(2, form.ssl_valid);
  s.BindInt(3, form.preferred);
  s.BindString(4, form.origin.spec());
  s.BindString16(5, form.username_element);
  s.BindString16(6, form.username_value);
  s.BindString16(7, form.password_element);
  s.BindString(8, form.signon_realm);

  if (!s.Run()) {
    NOTREACHED();
    return false;
  }
  return true;
}

bool WebDatabase::RemoveLogin(const PasswordForm& form) {
  // Remove a login by UNIQUE-constrained fields.
  sql::Statement s(db_.GetUniqueStatement(
      "DELETE FROM logins WHERE "
      "origin_url = ? AND "
      "username_element = ? AND "
      "username_value = ? AND "
      "password_element = ? AND "
      "submit_element = ? AND "
      "signon_realm = ?"));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }
  s.BindString(0, form.origin.spec());
  s.BindString16(1, form.username_element);
  s.BindString16(2, form.username_value);
  s.BindString16(3, form.password_element);
  s.BindString16(4, form.submit_element);
  s.BindString(5, form.signon_realm);

  if (!s.Run()) {
    NOTREACHED();
    return false;
  }
  return true;
}

bool WebDatabase::RemoveLoginsCreatedBetween(base::Time delete_begin,
                                             base::Time delete_end) {
  sql::Statement s1(db_.GetUniqueStatement(
      "DELETE FROM logins WHERE "
      "date_created >= ? AND date_created < ?"));
  if (!s1) {
    NOTREACHED() << "Statement 1 prepare failed";
    return false;
  }
  s1.BindInt64(0, delete_begin.ToTimeT());
  s1.BindInt64(1,
               delete_end.is_null() ?
                   std::numeric_limits<int64>::max() :
                   delete_end.ToTimeT());
  bool success = s1.Run();

#if defined(OS_WIN)
  sql::Statement s2(db_.GetUniqueStatement(
      "DELETE FROM ie7_logins WHERE date_created >= ? AND date_created < ?"));
  if (!s2) {
    NOTREACHED() << "Statement 2 prepare failed";
    return false;
  }
  s2.BindInt64(0, delete_begin.ToTimeT());
  s2.BindInt64(1,
               delete_end.is_null() ?
                   std::numeric_limits<int64>::max() :
                   delete_end.ToTimeT());
  success = success && s2.Run();
#endif

  return success;
}

bool WebDatabase::GetLogins(const PasswordForm& form,
                            std::vector<PasswordForm*>* forms) {
  DCHECK(forms);
  sql::Statement s(db_.GetUniqueStatement(
                "SELECT origin_url, action_url, "
                "username_element, username_value, "
                "password_element, password_value, "
                "submit_element, signon_realm, "
                "ssl_valid, preferred, "
                "date_created, blacklisted_by_user, scheme FROM logins "
                "WHERE signon_realm == ?"));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  s.BindString(0, form.signon_realm);

  while (s.Step()) {
    PasswordForm* new_form = new PasswordForm();
    InitPasswordFormFromStatement(new_form, &s);

    forms->push_back(new_form);
  }
  return s.Succeeded();
}

bool WebDatabase::GetAllLogins(std::vector<PasswordForm*>* forms,
                               bool include_blacklisted) {
  DCHECK(forms);
  std::string stmt = "SELECT origin_url, action_url, "
                     "username_element, username_value, "
                     "password_element, password_value, "
                     "submit_element, signon_realm, ssl_valid, preferred, "
                     "date_created, blacklisted_by_user, scheme FROM logins ";
  if (!include_blacklisted)
    stmt.append("WHERE blacklisted_by_user == 0 ");
  stmt.append("ORDER BY origin_url");

  sql::Statement s(db_.GetUniqueStatement(stmt.c_str()));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  while (s.Step()) {
    PasswordForm* new_form = new PasswordForm();
    InitPasswordFormFromStatement(new_form, &s);

    forms->push_back(new_form);
  }
  return s.Succeeded();
}

bool WebDatabase::AddFormFieldValues(const std::vector<FormField>& elements,
                                     std::vector<AutofillChange>* changes) {
  return AddFormFieldValuesTime(elements, changes, Time::Now());
}

bool WebDatabase::AddFormFieldValuesTime(const std::vector<FormField>& elements,
                                         std::vector<AutofillChange>* changes,
                                         base::Time time) {
  // Only add one new entry for each unique element name.  Use |seen_names| to
  // track this.  Add up to |kMaximumUniqueNames| unique entries per form.
  const size_t kMaximumUniqueNames = 256;
  std::set<string16> seen_names;
  bool result = true;
  for (std::vector<FormField>::const_iterator
       itr = elements.begin();
       itr != elements.end();
       itr++) {
    if (seen_names.size() >= kMaximumUniqueNames)
      break;
    if (seen_names.find(itr->name) != seen_names.end())
      continue;
    result = result && AddFormFieldValueTime(*itr, changes, time);
    seen_names.insert(itr->name);
  }
  return result;
}

bool WebDatabase::ClearAutofillEmptyValueElements() {
  sql::Statement s(db_.GetUniqueStatement(
      "SELECT pair_id FROM autofill WHERE TRIM(value)= \"\""));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  std::set<int64> ids;
  while (s.Step())
    ids.insert(s.ColumnInt64(0));

  bool success = true;
  for (std::set<int64>::const_iterator iter = ids.begin(); iter != ids.end();
       ++iter) {
    if (!RemoveFormElementForID(*iter))
      success = false;
  }

  return success;
}

bool WebDatabase::GetIDAndCountOfFormElement(
    const FormField& element,
    int64* pair_id,
    int* count) {
  sql::Statement s(db_.GetUniqueStatement(
      "SELECT pair_id, count FROM autofill "
      "WHERE name = ? AND value = ?"));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  s.BindString16(0, element.name);
  s.BindString16(1, element.value);

  *pair_id = 0;
  *count = 0;

  if (s.Step()) {
    *pair_id = s.ColumnInt64(0);
    *count = s.ColumnInt(1);
  }

  return true;
}

bool WebDatabase::GetCountOfFormElement(int64 pair_id, int* count) {
  sql::Statement s(db_.GetUniqueStatement(
      "SELECT count FROM autofill WHERE pair_id = ?"));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  s.BindInt64(0, pair_id);

  if (s.Step()) {
    *count = s.ColumnInt(0);
    return true;
  }
  return false;
}

bool WebDatabase::GetAllAutofillEntries(std::vector<AutofillEntry>* entries) {
  DCHECK(entries);
  sql::Statement s(db_.GetUniqueStatement(
      "SELECT name, value, date_created FROM autofill a JOIN "
      "autofill_dates ad ON a.pair_id=ad.pair_id"));

  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  bool first_entry = true;
  AutofillKey* current_key_ptr = NULL;
  std::vector<base::Time>* timestamps_ptr = NULL;
  string16 name, value;
  base::Time time;
  while (s.Step()) {
    name = s.ColumnString16(0);
    value = s.ColumnString16(1);
    time = Time::FromTimeT(s.ColumnInt64(2));

    if (first_entry) {
      current_key_ptr = new AutofillKey(name, value);

      timestamps_ptr = new std::vector<base::Time>;
      timestamps_ptr->push_back(time);

      first_entry = false;
    } else {
      // we've encountered the next entry
      if (current_key_ptr->name().compare(name) != 0 ||
          current_key_ptr->value().compare(value) != 0) {
        AutofillEntry entry(*current_key_ptr, *timestamps_ptr);
        entries->push_back(entry);

        delete current_key_ptr;
        delete timestamps_ptr;

        current_key_ptr = new AutofillKey(name, value);
        timestamps_ptr = new std::vector<base::Time>;
      }
      timestamps_ptr->push_back(time);
    }
  }
  // If there is at least one result returned, first_entry will be false.
  // For this case we need to do a final cleanup step.
  if (!first_entry) {
    AutofillEntry entry(*current_key_ptr, *timestamps_ptr);
    entries->push_back(entry);
    delete current_key_ptr;
    delete timestamps_ptr;
  }

  return s.Succeeded();
}

bool WebDatabase::GetAutofillTimestamps(const string16& name,
                                        const string16& value,
                                        std::vector<base::Time>* timestamps) {
  DCHECK(timestamps);
  sql::Statement s(db_.GetUniqueStatement(
      "SELECT date_created FROM autofill a JOIN "
      "autofill_dates ad ON a.pair_id=ad.pair_id "
      "WHERE a.name = ? AND a.value = ?"));

  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  s.BindString16(0, name);
  s.BindString16(1, value);
  while (s.Step()) {
    timestamps->push_back(Time::FromTimeT(s.ColumnInt64(0)));
  }

  return s.Succeeded();
}

bool WebDatabase::UpdateAutofillEntries(
    const std::vector<AutofillEntry>& entries) {
  if (!entries.size())
    return true;

  // Remove all existing entries.
  for (size_t i = 0; i < entries.size(); i++) {
    std::string sql = "SELECT pair_id FROM autofill "
                      "WHERE name = ? AND value = ?";
    sql::Statement s(db_.GetUniqueStatement(sql.c_str()));
    if (!s.is_valid()) {
      NOTREACHED() << "Statement prepare failed";
      return false;
    }

    s.BindString16(0, entries[i].key().name());
    s.BindString16(1, entries[i].key().value());
    if (s.Step()) {
      if (!RemoveFormElementForID(s.ColumnInt64(0)))
        return false;
    }
  }

  // Insert all the supplied autofill entries.
  for (size_t i = 0; i < entries.size(); i++) {
    if (!InsertAutofillEntry(entries[i]))
      return false;
  }

  return true;
}

bool WebDatabase::InsertAutofillEntry(const AutofillEntry& entry) {
  std::string sql = "INSERT INTO autofill (name, value, value_lower, count) "
                    "VALUES (?, ?, ?, ?)";
  sql::Statement s(db_.GetUniqueStatement(sql.c_str()));
  if (!s.is_valid()) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  s.BindString16(0, entry.key().name());
  s.BindString16(1, entry.key().value());
  s.BindString16(2, l10n_util::ToLower(entry.key().value()));
  s.BindInt(3, entry.timestamps().size());

  if (!s.Run()) {
    NOTREACHED();
    return false;
  }

  int64 pair_id = db_.GetLastInsertRowId();
  for (size_t i = 0; i < entry.timestamps().size(); i++) {
    if (!InsertPairIDAndDate(pair_id, entry.timestamps()[i]))
      return false;
  }

  return true;
}

bool WebDatabase::InsertFormElement(const FormField& element,
                                    int64* pair_id) {
  sql::Statement s(db_.GetUniqueStatement(
      "INSERT INTO autofill (name, value, value_lower) VALUES (?,?,?)"));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  s.BindString16(0, element.name);
  s.BindString16(1, element.value);
  s.BindString16(2, l10n_util::ToLower(element.value));

  if (!s.Run()) {
    NOTREACHED();
    return false;
  }

  *pair_id = db_.GetLastInsertRowId();
  return true;
}

bool WebDatabase::InsertPairIDAndDate(int64 pair_id,
                                      base::Time date_created) {
  sql::Statement s(db_.GetUniqueStatement(
      "INSERT INTO autofill_dates "
      "(pair_id, date_created) VALUES (?, ?)"));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  s.BindInt64(0, pair_id);
  s.BindInt64(1, date_created.ToTimeT());

  if (!s.Run()) {
    NOTREACHED();
    return false;
  }

  return true;
}

bool WebDatabase::SetCountOfFormElement(int64 pair_id, int count) {
  sql::Statement s(db_.GetUniqueStatement(
      "UPDATE autofill SET count = ? WHERE pair_id = ?"));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  s.BindInt(0, count);
  s.BindInt64(1, pair_id);
  if (!s.Run()) {
    NOTREACHED();
    return false;
  }

  return true;
}

bool WebDatabase::AddFormFieldValue(const FormField& element,
                                    std::vector<AutofillChange>* changes) {
  return AddFormFieldValueTime(element, changes, base::Time::Now());
}

bool WebDatabase::AddFormFieldValueTime(const FormField& element,
                                        std::vector<AutofillChange>* changes,
                                        base::Time time) {
  int count = 0;
  int64 pair_id;

  if (!GetIDAndCountOfFormElement(element, &pair_id, &count))
    return false;

  if (count == 0 && !InsertFormElement(element, &pair_id))
    return false;

  if (!SetCountOfFormElement(pair_id, count + 1))
    return false;

  if (!InsertPairIDAndDate(pair_id, time))
    return false;

  AutofillChange::Type change_type =
      count == 0 ? AutofillChange::ADD : AutofillChange::UPDATE;
  changes->push_back(
      AutofillChange(change_type,
                     AutofillKey(element.name, element.value)));
  return true;
}

bool WebDatabase::GetFormValuesForElementName(const string16& name,
                                              const string16& prefix,
                                              std::vector<string16>* values,
                                              int limit) {
  DCHECK(values);
  sql::Statement s;

  if (prefix.empty()) {
    s.Assign(db_.GetUniqueStatement(
        "SELECT value FROM autofill "
        "WHERE name = ? "
        "ORDER BY count DESC "
        "LIMIT ?"));
    if (!s) {
      NOTREACHED() << "Statement prepare failed";
      return false;
    }

    s.BindString16(0, name);
    s.BindInt(1, limit);
  } else {
    string16 prefix_lower = l10n_util::ToLower(prefix);
    string16 next_prefix = prefix_lower;
    next_prefix[next_prefix.length() - 1]++;

    s.Assign(db_.GetUniqueStatement(
        "SELECT value FROM autofill "
        "WHERE name = ? AND "
        "value_lower >= ? AND "
        "value_lower < ? "
        "ORDER BY count DESC "
        "LIMIT ?"));
    if (!s) {
      NOTREACHED() << "Statement prepare failed";
      return false;
    }

    s.BindString16(0, name);
    s.BindString16(1, prefix_lower);
    s.BindString16(2, next_prefix);
    s.BindInt(3, limit);
  }

  values->clear();
  while (s.Step())
    values->push_back(s.ColumnString16(0));
  return s.Succeeded();
}

bool WebDatabase::RemoveFormElementsAddedBetween(
    base::Time delete_begin,
    base::Time delete_end,
    std::vector<AutofillChange>* changes) {
  DCHECK(changes);
  // Query for the pair_id, name, and value of all form elements that
  // were used between the given times.
  sql::Statement s(db_.GetUniqueStatement(
      "SELECT DISTINCT a.pair_id, a.name, a.value "
      "FROM autofill_dates ad JOIN autofill a ON ad.pair_id = a.pair_id "
      "WHERE ad.date_created >= ? AND ad.date_created < ?"));
  if (!s) {
    NOTREACHED() << "Statement 1 prepare failed";
    return false;
  }
  s.BindInt64(0, delete_begin.ToTimeT());
  s.BindInt64(1,
              delete_end.is_null() ?
                  std::numeric_limits<int64>::max() :
                  delete_end.ToTimeT());

  AutofillElementList elements;
  while (s.Step()) {
    elements.push_back(MakeTuple(s.ColumnInt64(0),
                                 s.ColumnString16(1),
                                 s.ColumnString16(2)));
  }

  if (!s.Succeeded()) {
    NOTREACHED();
    return false;
  }

  for (AutofillElementList::iterator itr = elements.begin();
       itr != elements.end(); itr++) {
    int how_many = 0;
    if (!RemoveFormElementForTimeRange(itr->a, delete_begin, delete_end,
                                       &how_many)) {
      return false;
    }
    bool was_removed = false;
    if (!AddToCountOfFormElement(itr->a, -how_many, &was_removed))
      return false;
    AutofillChange::Type change_type =
        was_removed ? AutofillChange::REMOVE : AutofillChange::UPDATE;
    changes->push_back(AutofillChange(change_type,
                                      AutofillKey(itr->b, itr->c)));
  }

  return true;
}

bool WebDatabase::RemoveFormElementForTimeRange(int64 pair_id,
                                                const Time delete_begin,
                                                const Time delete_end,
                                                int* how_many) {
  sql::Statement s(db_.GetUniqueStatement(
      "DELETE FROM autofill_dates WHERE pair_id = ? AND "
      "date_created >= ? AND date_created < ?"));
  if (!s) {
    NOTREACHED() << "Statement 1 prepare failed";
    return false;
  }
  s.BindInt64(0, pair_id);
  s.BindInt64(1, delete_begin.is_null() ? 0 : delete_begin.ToTimeT());
  s.BindInt64(2, delete_end.is_null() ? std::numeric_limits<int64>::max() :
                                        delete_end.ToTimeT());

  bool result = s.Run();
  if (how_many)
    *how_many = db_.GetLastChangeCount();

  return result;
}

bool WebDatabase::RemoveFormElement(const string16& name,
                                    const string16& value) {
  // Find the id for that pair.
  sql::Statement s(db_.GetUniqueStatement(
      "SELECT pair_id FROM autofill WHERE  name = ? AND value= ?"));
  if (!s) {
    NOTREACHED() << "Statement 1 prepare failed";
    return false;
  }
  s.BindString16(0, name);
  s.BindString16(1, value);

  if (s.Step())
    return RemoveFormElementForID(s.ColumnInt64(0));
  return false;
}

bool WebDatabase::AddAutofillProfile(const AutofillProfile& profile) {
  sql::Statement s(db_.GetUniqueStatement(
      "INSERT INTO autofill_profiles"
      "(guid, company_name, address_line_1, address_line_2, city, state,"
      " zipcode, country, country_code, date_modified)"
      "VALUES (?,?,?,?,?,?,?,?,?,?)"));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  BindAutofillProfileToStatement(profile, &s);

  if (!s.Run()) {
    NOTREACHED();
    return false;
  }

  if (!s.Succeeded())
    return false;

  return AddAutofillProfilePieces(profile.guid(), profile, &db_);
}

bool WebDatabase::GetAutofillProfile(const std::string& guid,
                                     AutofillProfile** profile) {
  DCHECK(guid::IsValidGUID(guid));
  DCHECK(profile);
  sql::Statement s(db_.GetUniqueStatement(
      "SELECT guid, company_name, address_line_1, address_line_2, city, state,"
      " zipcode, country, country_code, date_modified "
      "FROM autofill_profiles "
      "WHERE guid=?"));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  s.BindString(0, guid);
  if (!s.Step())
    return false;

  if (!s.Succeeded())
    return false;

  scoped_ptr<AutofillProfile> p(AutofillProfileFromStatement(s));

  // Get associated name info.
  sql::Statement s2(db_.GetUniqueStatement(
      "SELECT guid, first_name, middle_name, last_name "
      "FROM autofill_profile_names "
      "WHERE guid=?"));
  if (!s2) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }
  s2.BindString(0, guid);

  if (s2.Step()) {
    AddAutofillProfileNameFromStatement(s2, p.get());
  }

  // Get associated email info.
  sql::Statement s3(db_.GetUniqueStatement(
      "SELECT guid, email "
      "FROM autofill_profile_emails "
      "WHERE guid=?"));
  if (!s3) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }
  s3.BindString(0, guid);

  if (s3.Step()) {
    AddAutofillProfileEmailFromStatement(s3, p.get());
  }

  // Get associated phone info.
  sql::Statement s4(db_.GetUniqueStatement(
      "SELECT guid, type, number "
      "FROM autofill_profile_phones "
      "WHERE guid=? AND type=?"));
  if (!s4) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }
  s4.BindString(0, guid);
  s4.BindInt(1, kAutoFillPhoneNumber);

  if (s4.Step()) {
    AddAutofillProfilePhoneFromStatement(s4, p.get());
  }

  // Get associated fax info.
  sql::Statement s5(db_.GetUniqueStatement(
      "SELECT guid, type, number "
      "FROM autofill_profile_phones "
      "WHERE guid=? AND type=?"));
  if (!s5) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }
  s5.BindString(0, guid);
  s5.BindInt(1, kAutoFillFaxNumber);

  if (s5.Step()) {
    AddAutofillProfileFaxFromStatement(s5, p.get());
  }

  *profile = p.release();
  return true;
}

bool WebDatabase::GetAutofillProfiles(
    std::vector<AutofillProfile*>* profiles) {
  DCHECK(profiles);
  profiles->clear();

  sql::Statement s(db_.GetUniqueStatement(
      "SELECT guid "
      "FROM autofill_profiles"));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  while (s.Step()) {
    std::string guid = s.ColumnString(0);
    AutofillProfile* profile = NULL;
    if (!GetAutofillProfile(guid, &profile))
      return false;
    profiles->push_back(profile);
  }

  return s.Succeeded();
}

bool WebDatabase::UpdateAutofillProfile(const AutofillProfile& profile) {
  DCHECK(guid::IsValidGUID(profile.guid()));

  AutofillProfile* tmp_profile = NULL;
  if (!GetAutofillProfile(profile.guid(), &tmp_profile))
    return false;

  // Preserve appropriate modification dates by not updating unchanged profiles.
  scoped_ptr<AutofillProfile> old_profile(tmp_profile);
  if (*old_profile == profile)
    return true;

  sql::Statement s(db_.GetUniqueStatement(
      "UPDATE autofill_profiles "
      "SET guid=?, company_name=?, address_line_1=?, address_line_2=?, "
      "    city=?, state=?, zipcode=?, country=?, country_code=?, "
      "    date_modified=? "
      "WHERE guid=?"));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  BindAutofillProfileToStatement(profile, &s);
  s.BindString(10, profile.guid());
  bool result = s.Run();
  DCHECK_GT(db_.GetLastChangeCount(), 0);
  if (!result)
    return result;

  // Remove the old names, emails, and phone/fax numbers.
  if (!RemoveAutofillProfilePieces(profile.guid(), &db_))
    return false;

  return AddAutofillProfilePieces(profile.guid(), profile, &db_);
}

bool WebDatabase::RemoveAutofillProfile(const std::string& guid) {
  DCHECK(guid::IsValidGUID(guid));
  sql::Statement s(db_.GetUniqueStatement(
      "DELETE FROM autofill_profiles WHERE guid = ?"));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  s.BindString(0, guid);
  if (!s.Run())
    return false;

  return RemoveAutofillProfilePieces(guid, &db_);
}

bool WebDatabase::AddCreditCard(const CreditCard& credit_card) {
  sql::Statement s(db_.GetUniqueStatement(
      "INSERT INTO credit_cards"
      "(guid, name_on_card, expiration_month, expiration_year, "
      "card_number_encrypted, date_modified)"
      "VALUES (?,?,?,?,?,?)"));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  BindCreditCardToStatement(credit_card, &s);

  if (!s.Run()) {
    NOTREACHED();
    return false;
  }

  DCHECK_GT(db_.GetLastChangeCount(), 0);
  return s.Succeeded();
}

bool WebDatabase::GetCreditCard(const std::string& guid,
                                CreditCard** credit_card) {
  DCHECK(guid::IsValidGUID(guid));
  sql::Statement s(db_.GetUniqueStatement(
      "SELECT guid, name_on_card, expiration_month, expiration_year, "
      "card_number_encrypted, date_modified "
      "FROM credit_cards "
      "WHERE guid = ?"));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  s.BindString(0, guid);
  if (!s.Step())
    return false;

  *credit_card = CreditCardFromStatement(s);

  return s.Succeeded();
}

bool WebDatabase::GetCreditCards(
    std::vector<CreditCard*>* credit_cards) {
  DCHECK(credit_cards);
  credit_cards->clear();

  sql::Statement s(db_.GetUniqueStatement(
      "SELECT guid "
      "FROM credit_cards"));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  while (s.Step()) {
    std::string guid = s.ColumnString(0);
    CreditCard* credit_card = NULL;
    if (!GetCreditCard(guid, &credit_card))
      return false;
    credit_cards->push_back(credit_card);
  }

  return s.Succeeded();
}

bool WebDatabase::UpdateCreditCard(const CreditCard& credit_card) {
  DCHECK(guid::IsValidGUID(credit_card.guid()));

  CreditCard* tmp_credit_card = NULL;
  if (!GetCreditCard(credit_card.guid(), &tmp_credit_card))
    return false;

  // Preserve appropriate modification dates by not updating unchanged cards.
  scoped_ptr<CreditCard> old_credit_card(tmp_credit_card);
  if (*old_credit_card == credit_card)
    return true;

  sql::Statement s(db_.GetUniqueStatement(
      "UPDATE credit_cards "
      "SET guid=?, name_on_card=?, expiration_month=?, "
      "    expiration_year=?, card_number_encrypted=?, date_modified=? "
      "WHERE guid=?"));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  BindCreditCardToStatement(credit_card, &s);
  s.BindString(6, credit_card.guid());
  bool result = s.Run();
  DCHECK_GT(db_.GetLastChangeCount(), 0);
  return result;
}

bool WebDatabase::RemoveCreditCard(const std::string& guid) {
  DCHECK(guid::IsValidGUID(guid));
  sql::Statement s(db_.GetUniqueStatement(
      "DELETE FROM credit_cards WHERE guid = ?"));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  s.BindString(0, guid);
  return s.Run();
}

bool WebDatabase::RemoveAutofillProfilesAndCreditCardsModifiedBetween(
    base::Time delete_begin,
    base::Time delete_end) {
  DCHECK(delete_end.is_null() || delete_begin < delete_end);

  time_t delete_begin_t = delete_begin.ToTimeT();
  time_t delete_end_t = delete_end.is_null() ?
      std::numeric_limits<time_t>::max() :
      delete_end.ToTimeT();

  // Remove AutoFill profiles in the time range.
  sql::Statement s_profiles(db_.GetUniqueStatement(
      "DELETE FROM autofill_profiles "
      "WHERE date_modified >= ? AND date_modified < ?"));
  if (!s_profiles) {
    NOTREACHED() << "AutoFill profiles statement prepare failed";
    return false;
  }

  s_profiles.BindInt64(0, delete_begin_t);
  s_profiles.BindInt64(1, delete_end_t);
  s_profiles.Run();

  if (!s_profiles.Succeeded()) {
    NOTREACHED();
    return false;
  }

  // Remove AutoFill profiles in the time range.
  sql::Statement s_credit_cards(db_.GetUniqueStatement(
      "DELETE FROM credit_cards "
      "WHERE date_modified >= ? AND date_modified < ?"));
  if (!s_credit_cards) {
    NOTREACHED() << "AutoFill credit cards statement prepare failed";
    return false;
  }

  s_credit_cards.BindInt64(0, delete_begin_t);
  s_credit_cards.BindInt64(1, delete_end_t);
  s_credit_cards.Run();

  if (!s_credit_cards.Succeeded()) {
    NOTREACHED();
    return false;
  }

  return true;
}

bool WebDatabase::AddToCountOfFormElement(int64 pair_id,
                                          int delta,
                                          bool* was_removed) {
  DCHECK(was_removed);
  int count = 0;
  *was_removed = false;

  if (!GetCountOfFormElement(pair_id, &count))
    return false;

  if (count + delta == 0) {
    if (!RemoveFormElementForID(pair_id))
      return false;
    *was_removed = true;
  } else {
    if (!SetCountOfFormElement(pair_id, count + delta))
      return false;
  }
  return true;
}

bool WebDatabase::RemoveFormElementForID(int64 pair_id) {
  sql::Statement s(db_.GetUniqueStatement(
      "DELETE FROM autofill WHERE pair_id = ?"));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }
  s.BindInt64(0, pair_id);
  if (s.Run()) {
    return RemoveFormElementForTimeRange(pair_id, base::Time(), base::Time(),
                                         NULL);
  }
  return false;
}

sql::InitStatus WebDatabase::MigrateOldVersionsAsNeeded(){
  // Migrate if necessary.
  int current_version = meta_table_.GetVersionNumber();
  switch (current_version) {
    // Versions 1 - 19 are unhandled.  Version numbers greater than
    // kCurrentVersionNumber should have already been weeded out by the caller.
    default:
      // When the version is too old, we return failure error code.  The schema
      // is too out of date to migrate.
      // There should not be a released product that makes a database too old to
      // migrate. If we do encounter such a legacy database, we will need a
      // better solution to handle it (i.e., pop up a dialog to tell the user,
      // erase all their prefs and start over, etc.).
      LOG(WARNING) << "Web database version " << current_version <<
          " is too old to handle.";
      NOTREACHED();
      return sql::INIT_FAILURE;

    case 20:
      // Add the autogenerate_keyword column.
      if (!db_.Execute("ALTER TABLE keywords ADD COLUMN autogenerate_keyword "
                       "INTEGER DEFAULT 0")) {
        LOG(WARNING) << "Unable to update web database to version 21.";
        NOTREACHED();
        return sql::INIT_FAILURE;
      }
      meta_table_.SetVersionNumber(21);
      meta_table_.SetCompatibleVersionNumber(
          std::min(21, kCompatibleVersionNumber));
      // FALL THROUGH

    case 21:
      if (!ClearAutofillEmptyValueElements()) {
        LOG(WARNING) << "Failed to clean-up autofill DB.";
        NOTREACHED();
        return sql::INIT_FAILURE;
      }
      meta_table_.SetVersionNumber(22);
      // No change in the compatibility version number.

      // FALL THROUGH

    case 22:
      // Add the card_number_encrypted column if credit card table was not
      // created in this build (otherwise the column already exists).
      // WARNING: Do not change the order of the execution of the SQL
      // statements in this case!  Profile corruption and data migration
      // issues WILL OCCUR. (see http://crbug.com/10913)
      //
      // The problem is that if a user has a profile which was created before
      // r37036, when the credit_cards table was added, and then failed to
      // update this profile between the credit card addition and the addition
      // of the "encrypted" columns (44963), the next data migration will put
      // the user's profile in an incoherent state: The user will update from
      // a data profile set to be earlier than 22, and therefore pass through
      // this update case.  But because the user did not have a credit_cards
      // table before starting Chrome, it will have just been initialized
      // above, and so already have these columns -- and thus this data
      // update step will have failed.
      //
      // The false assumption in this case is that at this step in the
      // migration, the user has a credit card table, and that this
      // table does not include encrypted columns!
      // Because this case does not roll back the complete set of SQL
      // transactions properly in case of failure (that is, it does not
      // roll back the table initialization done above), the incoherent
      // profile will now see itself as being at version 22 -- but include a
      // fully initialized credit_cards table.  Every time Chrome runs, it
      // will try to update the web database and fail at this step, unless
      // we allow for the faulty assumption described above by checking for
      // the existence of the columns only AFTER we've executed the commands
      // to add them.
      if (!db_.DoesColumnExist("credit_cards", "card_number_encrypted")) {
        if (!db_.Execute("ALTER TABLE credit_cards ADD COLUMN "
                         "card_number_encrypted BLOB DEFAULT NULL")) {
          LOG(WARNING) << "Could not add card_number_encrypted to "
                          "credit_cards table.";
          NOTREACHED();
          return sql::INIT_FAILURE;
        }
      }

      if (!db_.DoesColumnExist("credit_cards", "verification_code_encrypted")) {
        if (!db_.Execute("ALTER TABLE credit_cards ADD COLUMN "
                         "verification_code_encrypted BLOB DEFAULT NULL")) {
          LOG(WARNING) << "Could not add verification_code_encrypted to "
                          "credit_cards table.";
          NOTREACHED();
          return sql::INIT_FAILURE;
        }
      }
      meta_table_.SetVersionNumber(23);
      // FALL THROUGH

    case 23: {
      // One-time cleanup for Chromium bug 38364.  In the presence of
      // multi-byte UTF-8 characters, that bug could cause AutoFill strings
      // to grow larger and more corrupt with each save.  The cleanup removes
      // any row with a string field larger than a reasonable size.  The string
      // fields examined here are precisely the ones that were subject to
      // corruption by the original bug.
      const std::string autofill_is_too_big =
          "max(length(name), length(value)) > 500";

      const std::string credit_cards_is_too_big =
          "max(length(label), length(name_on_card), length(type), "
          "    length(expiration_month), length(expiration_year), "
          "    length(billing_address), length(shipping_address) "
          ") > 500";

      const std::string autofill_profiles_is_too_big =
          "max(length(label), length(first_name), "
          "    length(middle_name), length(last_name), length(email), "
          "    length(company_name), length(address_line_1), "
          "    length(address_line_2), length(city), length(state), "
          "    length(zipcode), length(country), length(phone), "
          "    length(fax)) > 500";

      std::string query = "DELETE FROM autofill_dates WHERE pair_id IN ("
          "SELECT pair_id FROM autofill WHERE " + autofill_is_too_big + ")";
      if (!db_.Execute(query.c_str())) {
        LOG(WARNING) << "Unable to update web database to version 24.";
        NOTREACHED();
        return sql::INIT_FAILURE;
      }
      query = "DELETE FROM autofill WHERE " + autofill_is_too_big;
      if (!db_.Execute(query.c_str())) {
        LOG(WARNING) << "Unable to update web database to version 24.";
        NOTREACHED();
        return sql::INIT_FAILURE;
      }
      // Only delete from legacy credit card tables where specific columns
      // exist.
      if (db_.DoesColumnExist("credit_cards", "label") &&
          db_.DoesColumnExist("credit_cards", "name_on_card") &&
          db_.DoesColumnExist("credit_cards", "type") &&
          db_.DoesColumnExist("credit_cards", "expiration_month") &&
          db_.DoesColumnExist("credit_cards", "expiration_year") &&
          db_.DoesColumnExist("credit_cards", "billing_address") &&
          db_.DoesColumnExist("credit_cards", "shipping_address") &&
          db_.DoesColumnExist("autofill_profiles", "label")) {
        query = "DELETE FROM credit_cards WHERE (" + credit_cards_is_too_big +
            ") OR label IN (SELECT label FROM autofill_profiles WHERE " +
            autofill_profiles_is_too_big + ")";
        if (!db_.Execute(query.c_str())) {
          LOG(WARNING) << "Unable to update web database to version 24.";
          NOTREACHED();
          return sql::INIT_FAILURE;
        }
      }
      if (db_.DoesColumnExist("autofill_profiles", "label")) {
        query = "DELETE FROM autofill_profiles WHERE " +
            autofill_profiles_is_too_big;
        if (!db_.Execute(query.c_str())) {
          LOG(WARNING) << "Unable to update web database to version 24.";
          NOTREACHED();
          return sql::INIT_FAILURE;
        }
      }

      meta_table_.SetVersionNumber(24);

      // FALL THROUGH
    }

    case 24:
      // Add the logo_id column if keyword table was not created in this build.
      if (!db_.Execute("ALTER TABLE keywords ADD COLUMN logo_id "
                       "INTEGER DEFAULT 0")) {
        LOG(WARNING) << "Unable to update web database to version 25.";
        NOTREACHED();
        return sql::INIT_FAILURE;
      }
      meta_table_.SetVersionNumber(25);
      meta_table_.SetCompatibleVersionNumber(
          std::min(25, kCompatibleVersionNumber));
      // FALL THROUGH

    case 25:
      // Add the created_by_policy column.
      if (!db_.Execute("ALTER TABLE keywords ADD COLUMN created_by_policy "
                       "INTEGER DEFAULT 0")) {
        LOG(WARNING) << "Unable to update web database to version 26.";
        NOTREACHED();
        return sql::INIT_FAILURE;
      }

      meta_table_.SetVersionNumber(26);
      meta_table_.SetCompatibleVersionNumber(
          std::min(26, kCompatibleVersionNumber));
      // FALL THROUGH

    case 26: {
      // Only migrate from legacy credit card tables where specific columns
      // exist.
      if (db_.DoesColumnExist("credit_cards", "unique_id") &&
          db_.DoesColumnExist("credit_cards", "billing_address") &&
          db_.DoesColumnExist("autofill_profiles", "unique_id")) {
        // Change the credit_cards.billing_address column from a string to an
        // int.  The stored string is the label of an address, so we have to
        // select the unique ID of this address using the label as a foreign
        // key into the |autofill_profiles| table.
        std::string stmt =
            "SELECT credit_cards.unique_id, autofill_profiles.unique_id "
            "FROM autofill_profiles, credit_cards "
            "WHERE credit_cards.billing_address = autofill_profiles.label";
        sql::Statement s(db_.GetUniqueStatement(stmt.c_str()));
        if (!s) {
          LOG(WARNING) << "Statement prepare failed";
          NOTREACHED();
          return sql::INIT_FAILURE;
        }

        std::map<int, int> cc_billing_map;
        while (s.Step())
          cc_billing_map[s.ColumnInt(0)] = s.ColumnInt(1);

        // Windows already stores the IDs as strings in |billing_address|. Try
        // to convert those.
        if (cc_billing_map.empty()) {
          std::string stmt =
            "SELECT unique_id,billing_address FROM credit_cards";
          sql::Statement s(db_.GetUniqueStatement(stmt.c_str()));
          if (!s) {
            LOG(WARNING) << "Statement prepare failed";
            NOTREACHED();
            return sql::INIT_FAILURE;
          }

          while (s.Step()) {
            int id = 0;
            if (base::StringToInt(s.ColumnString(1), &id))
              cc_billing_map[s.ColumnInt(0)] = id;
          }
        }

        if (!db_.Execute("CREATE TABLE credit_cards_temp ( "
                         "label VARCHAR, "
                         "unique_id INTEGER PRIMARY KEY, "
                         "name_on_card VARCHAR, "
                         "type VARCHAR, "
                         "card_number VARCHAR, "
                         "expiration_month INTEGER, "
                         "expiration_year INTEGER, "
                         "verification_code VARCHAR, "
                         "billing_address INTEGER, "
                         "shipping_address VARCHAR, "
                         "card_number_encrypted BLOB, "
                         "verification_code_encrypted BLOB)")) {
          LOG(WARNING) << "Unable to update web database to version 27.";
          NOTREACHED();
          return sql::INIT_FAILURE;
        }

        if (!db_.Execute(
            "INSERT INTO credit_cards_temp "
            "SELECT label,unique_id,name_on_card,type,card_number,"
            "expiration_month,expiration_year,verification_code,0,"
            "shipping_address,card_number_encrypted,"
            "verification_code_encrypted FROM credit_cards")) {
          LOG(WARNING) << "Unable to update web database to version 27.";
          NOTREACHED();
          return sql::INIT_FAILURE;
        }

        if (!db_.Execute("DROP TABLE credit_cards")) {
          LOG(WARNING) << "Unable to update web database to version 27.";
          NOTREACHED();
          return sql::INIT_FAILURE;
        }

        if (!db_.Execute(
            "ALTER TABLE credit_cards_temp RENAME TO credit_cards")) {
          LOG(WARNING) << "Unable to update web database to version 27.";
          NOTREACHED();
          return sql::INIT_FAILURE;
        }

        for (std::map<int, int>::const_iterator iter = cc_billing_map.begin();
             iter != cc_billing_map.end(); ++iter) {
          sql::Statement s(db_.GetCachedStatement(
              SQL_FROM_HERE,
              "UPDATE credit_cards SET billing_address=? WHERE unique_id=?"));
          if (!s) {
            LOG(WARNING) << "Statement prepare failed";
            NOTREACHED();
            return sql::INIT_FAILURE;
          }

          s.BindInt(0, (*iter).second);
          s.BindInt(1, (*iter).first);

          if (!s.Run()) {
            LOG(WARNING) << "Unable to update web database to version 27.";
            NOTREACHED();
            return sql::INIT_FAILURE;
          }
        }
      }

      meta_table_.SetVersionNumber(27);
      meta_table_.SetCompatibleVersionNumber(
          std::min(27, kCompatibleVersionNumber));

      // FALL THROUGH
    }

    case 27:
      // Add supports_instant to keywords.
      if (!db_.Execute("ALTER TABLE keywords ADD COLUMN supports_instant "
                       "INTEGER DEFAULT 0")) {
        LOG(WARNING) << "Unable to update web database to version 28.";
        NOTREACHED();
        return sql::INIT_FAILURE;
      }
      meta_table_.SetVersionNumber(28);
      meta_table_.SetCompatibleVersionNumber(
          std::min(28, kCompatibleVersionNumber));

      // FALL THROUGH

    case 28:
      // Keywords loses the column supports_instant and gets instant_url.
      if (!db_.Execute("ALTER TABLE keywords ADD COLUMN instant_url "
                       "VARCHAR")) {
        LOG(WARNING) << "Unable to update web database to version 29.";
        NOTREACHED();
        return sql::INIT_FAILURE;
      }
      if (!db_.Execute("CREATE TABLE keywords_temp ("
                       "id INTEGER PRIMARY KEY,"
                       "short_name VARCHAR NOT NULL,"
                       "keyword VARCHAR NOT NULL,"
                       "favicon_url VARCHAR NOT NULL,"
                       "url VARCHAR NOT NULL,"
                       "show_in_default_list INTEGER,"
                       "safe_for_autoreplace INTEGER,"
                       "originating_url VARCHAR,"
                       "date_created INTEGER DEFAULT 0,"
                       "usage_count INTEGER DEFAULT 0,"
                       "input_encodings VARCHAR,"
                       "suggest_url VARCHAR,"
                       "prepopulate_id INTEGER DEFAULT 0,"
                       "autogenerate_keyword INTEGER DEFAULT 0,"
                       "logo_id INTEGER DEFAULT 0,"
                       "created_by_policy INTEGER DEFAULT 0,"
                       "instant_url VARCHAR)")) {
        LOG(WARNING) << "Unable to update web database to version 29.";
        NOTREACHED();
        return sql::INIT_FAILURE;
      }

      if (!db_.Execute(
          "INSERT INTO keywords_temp "
          "SELECT id, short_name, keyword, favicon_url, url, "
          "show_in_default_list, safe_for_autoreplace, originating_url, "
          "date_created, usage_count, input_encodings, suggest_url, "
          "prepopulate_id, autogenerate_keyword, logo_id, created_by_policy, "
          "instant_url FROM keywords")) {
        LOG(WARNING) << "Unable to update web database to version 29.";
        NOTREACHED();
        return sql::INIT_FAILURE;
      }

      if (!db_.Execute("DROP TABLE keywords")) {
        LOG(WARNING) << "Unable to update web database to version 29.";
        NOTREACHED();
        return sql::INIT_FAILURE;
      }

      if (!db_.Execute(
          "ALTER TABLE keywords_temp RENAME TO keywords")) {
        LOG(WARNING) << "Unable to update web database to version 29.";
        NOTREACHED();
        return sql::INIT_FAILURE;
      }

      meta_table_.SetVersionNumber(29);
      meta_table_.SetCompatibleVersionNumber(
          std::min(29, kCompatibleVersionNumber));

      // FALL THROUGH

    case 29:
      // Add date_modified to autofill_profiles.
      if (!db_.DoesColumnExist("autofill_profiles", "date_modified")) {
        if (!db_.Execute("ALTER TABLE autofill_profiles ADD COLUMN "
                         "date_modified INTEGER NON NULL DEFAULT 0")) {
          LOG(WARNING) << "Unable to update web database to version 30";
          NOTREACHED();
          return sql::INIT_FAILURE;
        }

        sql::Statement s(db_.GetUniqueStatement(
            "UPDATE autofill_profiles SET date_modified=?"));
        if (!s) {
          LOG(WARNING) << "Unable to update web database to version 30.";
          NOTREACHED();
          return sql::INIT_FAILURE;
        }

        s.BindInt64(0, Time::Now().ToTimeT());

        if (!s.Run()) {
          LOG(WARNING) << "Unable to update web database to version 30.";
          NOTREACHED();
          return sql::INIT_FAILURE;
        }

      }

      // Add date_modified to credit_cards.
      if (!db_.DoesColumnExist("credit_cards", "date_modified")) {
        if (!db_.Execute("ALTER TABLE credit_cards ADD COLUMN "
                         "date_modified INTEGER NON NULL DEFAULT 0")) {
          LOG(WARNING) << "Unable to update web database to version 30";
          NOTREACHED();
          return sql::INIT_FAILURE;
        }

        sql::Statement s(db_.GetUniqueStatement(
            "UPDATE credit_cards SET date_modified=?"));
        if (!s) {
          LOG(WARNING) << "Unable to update web database to version 30.";
          NOTREACHED();
          return sql::INIT_FAILURE;
        }

        s.BindInt64(0, Time::Now().ToTimeT());

        if (!s.Run()) {
          LOG(WARNING) << "Unable to update web database to version 30.";
          NOTREACHED();
          return sql::INIT_FAILURE;
        }
      }

      meta_table_.SetVersionNumber(30);
      meta_table_.SetCompatibleVersionNumber(
          std::min(30, kCompatibleVersionNumber));

      // FALL THROUGH

    case 30:
      // Add |guid| column to |autofill_profiles| table.
      // Note that we need to check for the guid column's existence due to the
      // fact that for a version 22 database the |autofill_profiles| table
      // gets created fresh with |InitAutofillProfilesTable|.
      if (!db_.DoesColumnExist("autofill_profiles", "guid")) {
        if (!db_.Execute("ALTER TABLE autofill_profiles ADD COLUMN "
                         "guid VARCHAR NOT NULL DEFAULT \"\"")) {
          LOG(WARNING) << "Unable to update web database to version 30.";
          NOTREACHED();
          return sql::INIT_FAILURE;
        }

        // Set all the |guid| fields to valid values.
        {
          sql::Statement s(db_.GetUniqueStatement("SELECT unique_id "
                                                  "FROM autofill_profiles"));

          if (!s) {
            LOG(WARNING) << "Unable to update web database to version 30.";
            NOTREACHED();
            return sql::INIT_FAILURE;
          }

          while (s.Step()) {
            sql::Statement update_s(
                db_.GetUniqueStatement("UPDATE autofill_profiles "
                                       "SET guid=? WHERE unique_id=?"));
            if (!update_s) {
              LOG(WARNING) << "Unable to update web database to version 30.";
              NOTREACHED();
              return sql::INIT_FAILURE;
            }
            update_s.BindString(0, guid::GenerateGUID());
            update_s.BindInt(1, s.ColumnInt(0));

            if (!update_s.Run()) {
              LOG(WARNING) << "Unable to update web database to version 30.";
              NOTREACHED();
              return sql::INIT_FAILURE;
            }
          }
        }
      }

      // Add |guid| column to |credit_cards| table.
      // Note that we need to check for the guid column's existence due to the
      // fact that for a version 22 database the |autofill_profiles| table
      // gets created fresh with |InitAutofillProfilesTable|.
      if (!db_.DoesColumnExist("credit_cards", "guid")) {
        if (!db_.Execute("ALTER TABLE credit_cards ADD COLUMN "
                         "guid VARCHAR NOT NULL DEFAULT \"\"")) {
          LOG(WARNING) << "Unable to update web database to version 30.";
          NOTREACHED();
          return sql::INIT_FAILURE;
        }

        // Set all the |guid| fields to valid values.
        {
          sql::Statement s(db_.GetUniqueStatement("SELECT unique_id "
                                                  "FROM credit_cards"));
          if (!s) {
            LOG(WARNING) << "Unable to update web database to version 30.";
            NOTREACHED();
            return sql::INIT_FAILURE;
          }

          while (s.Step()) {
            sql::Statement update_s(
                db_.GetUniqueStatement("UPDATE credit_cards "
                                       "set guid=? WHERE unique_id=?"));
            if (!update_s) {
              LOG(WARNING) << "Unable to update web database to version 30.";
              NOTREACHED();
              return sql::INIT_FAILURE;
            }
            update_s.BindString(0, guid::GenerateGUID());
            update_s.BindInt(1, s.ColumnInt(0));

            if (!update_s.Run()) {
              LOG(WARNING) << "Unable to update web database to version 30.";
              NOTREACHED();
              return sql::INIT_FAILURE;
            }
          }
        }
      }

      meta_table_.SetVersionNumber(31);
      meta_table_.SetCompatibleVersionNumber(
          std::min(31, kCompatibleVersionNumber));

      // FALL THROUGH

    case 31:
      if (db_.DoesColumnExist("autofill_profiles", "unique_id")) {
        if (!db_.Execute("CREATE TABLE autofill_profiles_temp ( "
                         "guid VARCHAR PRIMARY KEY, "
                         "label VARCHAR, "
                         "first_name VARCHAR, "
                         "middle_name VARCHAR, "
                         "last_name VARCHAR, "
                         "email VARCHAR, "
                         "company_name VARCHAR, "
                         "address_line_1 VARCHAR, "
                         "address_line_2 VARCHAR, "
                         "city VARCHAR, "
                         "state VARCHAR, "
                         "zipcode VARCHAR, "
                         "country VARCHAR, "
                         "phone VARCHAR, "
                         "fax VARCHAR, "
                         "date_modified INTEGER NOT NULL DEFAULT 0)")) {
          LOG(WARNING) << "Unable to update web database to version 32.";
          NOTREACHED();
          return sql::INIT_FAILURE;
        }

        if (!db_.Execute(
            "INSERT INTO autofill_profiles_temp "
            "SELECT guid, label, first_name, middle_name, last_name, email, "
            "company_name, address_line_1, address_line_2, city, state, "
            "zipcode, country, phone, fax, date_modified "
            "FROM autofill_profiles")) {
          LOG(WARNING) << "Unable to update web database to version 32.";
          NOTREACHED();
          return sql::INIT_FAILURE;
        }

        if (!db_.Execute("DROP TABLE autofill_profiles")) {
          LOG(WARNING) << "Unable to update web database to version 32.";
          NOTREACHED();
          return sql::INIT_FAILURE;
        }

        if (!db_.Execute(
            "ALTER TABLE autofill_profiles_temp RENAME TO autofill_profiles")) {
          LOG(WARNING) << "Unable to update web database to version 32.";
          NOTREACHED();
          return sql::INIT_FAILURE;
        }
      }

      if (db_.DoesColumnExist("credit_cards", "unique_id")) {
        if (!db_.Execute("CREATE TABLE credit_cards_temp ( "
                         "guid VARCHAR PRIMARY KEY, "
                         "label VARCHAR, "
                         "name_on_card VARCHAR, "
                         "expiration_month INTEGER, "
                         "expiration_year INTEGER, "
                         "card_number_encrypted BLOB, "
                         "date_modified INTEGER NOT NULL DEFAULT 0)")) {
          LOG(WARNING) << "Unable to update web database to version 32.";
          NOTREACHED();
          return sql::INIT_FAILURE;
        }

        if (!db_.Execute(
            "INSERT INTO credit_cards_temp "
            "SELECT guid, label, name_on_card, expiration_month, "
            "expiration_year, card_number_encrypted, date_modified "
            "FROM credit_cards")) {
          LOG(WARNING) << "Unable to update web database to version 32.";
          NOTREACHED();
          return sql::INIT_FAILURE;
        }

        if (!db_.Execute("DROP TABLE credit_cards")) {
          LOG(WARNING) << "Unable to update web database to version 32.";
          NOTREACHED();
          return sql::INIT_FAILURE;
        }

        if (!db_.Execute(
            "ALTER TABLE credit_cards_temp RENAME TO credit_cards")) {
          LOG(WARNING) << "Unable to update web database to version 32.";
          NOTREACHED();
          return sql::INIT_FAILURE;
        }
      }

      meta_table_.SetVersionNumber(32);
      meta_table_.SetCompatibleVersionNumber(
          std::min(32, kCompatibleVersionNumber));

      // FALL THROUGH

    case 32:
      // Test the existence of the |first_name| column as an indication that
      // we need a migration.  It is possible that the new |autofill_profiles|
      // schema is in place because the table was newly created when migrating
      // from a pre-version-22 database.
      if (db_.DoesColumnExist("autofill_profiles", "first_name")) {
        // Create autofill_profiles_temp table that will receive the data.
        if (!db_.DoesTableExist("autofill_profiles_temp")) {
          if (!db_.Execute("CREATE TABLE autofill_profiles_temp ( "
                           "guid VARCHAR PRIMARY KEY, "
                           "company_name VARCHAR, "
                           "address_line_1 VARCHAR, "
                           "address_line_2 VARCHAR, "
                           "city VARCHAR, "
                           "state VARCHAR, "
                           "zipcode VARCHAR, "
                           "country VARCHAR, "
                           "date_modified INTEGER NOT NULL DEFAULT 0)")) {
            NOTREACHED();
            return sql::INIT_FAILURE;
          }
        }

        {
          sql::Statement s(db_.GetUniqueStatement(
              "SELECT guid, first_name, middle_name, last_name, email, "
              "company_name, address_line_1, address_line_2, city, state, "
              "zipcode, country, phone, fax, date_modified "
              "FROM autofill_profiles"));
          while (s.Step()) {
            AutofillProfile profile;
            profile.set_guid(s.ColumnString(0));
            DCHECK(guid::IsValidGUID(profile.guid()));

            profile.SetInfo(AutofillType(NAME_FIRST), s.ColumnString16(1));
            profile.SetInfo(AutofillType(NAME_MIDDLE), s.ColumnString16(2));
            profile.SetInfo(AutofillType(NAME_LAST), s.ColumnString16(3));
            profile.SetInfo(AutofillType(EMAIL_ADDRESS), s.ColumnString16(4));
            profile.SetInfo(AutofillType(COMPANY_NAME), s.ColumnString16(5));
            profile.SetInfo(AutofillType(ADDRESS_HOME_LINE1),
                            s.ColumnString16(6));
            profile.SetInfo(AutofillType(ADDRESS_HOME_LINE2),
                            s.ColumnString16(7));
            profile.SetInfo(AutofillType(ADDRESS_HOME_CITY),
                            s.ColumnString16(8));
            profile.SetInfo(AutofillType(ADDRESS_HOME_STATE),
                            s.ColumnString16(9));
            profile.SetInfo(AutofillType(ADDRESS_HOME_ZIP),
                            s.ColumnString16(10));
            profile.SetInfo(AutofillType(ADDRESS_HOME_COUNTRY),
                            s.ColumnString16(11));
            profile.SetInfo(AutofillType(PHONE_HOME_WHOLE_NUMBER),
                            s.ColumnString16(12));
            profile.SetInfo(AutofillType(PHONE_FAX_WHOLE_NUMBER),
                            s.ColumnString16(13));
            int64 date_modified = s.ColumnInt64(14);

            sql::Statement s_insert(db_.GetUniqueStatement(
                "INSERT INTO autofill_profiles_temp"
                "(guid, company_name, address_line_1, address_line_2, city,"
                " state, zipcode, country, date_modified)"
                "VALUES (?,?,?,?,?,?,?,?,?)"));
            if (!s) {
              LOG(WARNING) << "Unable to update web database to version 33.";
              NOTREACHED();
              return sql::INIT_FAILURE;
            }
            s_insert.BindString(0, profile.guid());
            s_insert.BindString16(
                1, profile.GetFieldText(AutofillType(COMPANY_NAME)));
            s_insert.BindString16(
                2, profile.GetFieldText(AutofillType(ADDRESS_HOME_LINE1)));
            s_insert.BindString16(
                3, profile.GetFieldText(AutofillType(ADDRESS_HOME_LINE2)));
            s_insert.BindString16(
                4, profile.GetFieldText(AutofillType(ADDRESS_HOME_CITY)));
            s_insert.BindString16(
                5, profile.GetFieldText(AutofillType(ADDRESS_HOME_STATE)));
            s_insert.BindString16(
                6, profile.GetFieldText(AutofillType(ADDRESS_HOME_ZIP)));
            s_insert.BindString16(
                7, profile.GetFieldText(AutofillType(ADDRESS_HOME_COUNTRY)));
            s_insert.BindInt64(8, date_modified);

            if (!s_insert.Run()) {
              NOTREACHED();
              return sql::INIT_FAILURE;
            }

            // Add the other bits: names, emails, and phone/fax.
            if (!AddAutofillProfilePieces(profile.guid(), profile, &db_)) {
              NOTREACHED();
              return sql::INIT_FAILURE;
            }
          }
        }

        if (!db_.Execute("DROP TABLE autofill_profiles")) {
          LOG(WARNING) << "Unable to update web database to version 33.";
          NOTREACHED();
          return sql::INIT_FAILURE;
        }

        if (!db_.Execute(
            "ALTER TABLE autofill_profiles_temp RENAME TO autofill_profiles")) {
          LOG(WARNING) << "Unable to update web database to version 33.";
          NOTREACHED();
          return sql::INIT_FAILURE;
        }
      }

      // Remove the labels column from the credit_cards table.
      if (db_.DoesColumnExist("credit_cards", "label")) {
        if (!db_.Execute("CREATE TABLE credit_cards_temp ( "
                         "guid VARCHAR PRIMARY KEY, "
                         "name_on_card VARCHAR, "
                         "expiration_month INTEGER, "
                         "expiration_year INTEGER, "
                         "card_number_encrypted BLOB, "
                         "date_modified INTEGER NOT NULL DEFAULT 0)")) {
          LOG(WARNING) << "Unable to update web database to version 33.";
          NOTREACHED();
          return sql::INIT_FAILURE;
        }

        if (!db_.Execute(
            "INSERT INTO credit_cards_temp "
            "SELECT guid, name_on_card, expiration_month, "
            "expiration_year, card_number_encrypted, date_modified "
            "FROM credit_cards")) {
          LOG(WARNING) << "Unable to update web database to version 33.";
          NOTREACHED();
          return sql::INIT_FAILURE;
        }

        if (!db_.Execute("DROP TABLE credit_cards")) {
          LOG(WARNING) << "Unable to update web database to version 33.";
          NOTREACHED();
          return sql::INIT_FAILURE;
        }

        if (!db_.Execute(
            "ALTER TABLE credit_cards_temp RENAME TO credit_cards")) {
          LOG(WARNING) << "Unable to update web database to version 33.";
          NOTREACHED();
          return sql::INIT_FAILURE;
        }
      }

      meta_table_.SetVersionNumber(33);
      meta_table_.SetCompatibleVersionNumber(
          std::min(33, kCompatibleVersionNumber));

      // FALL THROUGH

    case 33:
      // Test the existence of the |country_code| column as an indication that
      // we need a migration.  It is possible that the new |autofill_profiles|
      // schema is in place because the table was newly created when migrating
      // from a pre-version-22 database.
      if (!db_.DoesColumnExist("autofill_profiles", "country_code")) {
        if (!db_.Execute("ALTER TABLE autofill_profiles ADD COLUMN "
                         "country_code VARCHAR")) {
          LOG(WARNING) << "Unable to update web database to version 33.";
          NOTREACHED();
          return sql::INIT_FAILURE;
        }

        // Set all the |country_code| fields to match existing |country| values.
        {
          sql::Statement s(db_.GetUniqueStatement("SELECT guid, country "
                                                  "FROM autofill_profiles"));

          if (!s) {
            LOG(WARNING) << "Unable to update web database to version 33.";
            NOTREACHED();
            return sql::INIT_FAILURE;
          }

          while (s.Step()) {
            sql::Statement update_s(
                db_.GetUniqueStatement("UPDATE autofill_profiles "
                                       "SET country_code=? WHERE guid=?"));
            if (!update_s) {
              LOG(WARNING) << "Unable to update web database to version 33.";
              NOTREACHED();
              return sql::INIT_FAILURE;
            }
            string16 country = s.ColumnString16(1);
            std::string app_locale = AutofillCountry::ApplicationLocale();
            update_s.BindString(0, AutofillCountry::GetCountryCode(country,
                                                                   app_locale));
            update_s.BindString(1, s.ColumnString(0));

            if (!update_s.Run()) {
              LOG(WARNING) << "Unable to update web database to version 33.";
              NOTREACHED();
              return sql::INIT_FAILURE;
            }
          }
        }
      }

      meta_table_.SetVersionNumber(34);
      meta_table_.SetCompatibleVersionNumber(
          std::min(34, kCompatibleVersionNumber));

      // FALL THROUGH

    case 34:
      // Correct all country codes with value "UK" to be "GB".  This data
      // was mistakenly introduced in build 686.0.  This migration is to clean
      // it up.  See http://crbug.com/74511 for details.
      {
        sql::Statement s(db_.GetUniqueStatement(
            "UPDATE autofill_profiles SET country_code=\"GB\" "
            "WHERE country_code=\"UK\""));

        if (!s.Run()) {
          LOG(WARNING) << "Unable to update web database to version 35.";
          NOTREACHED();
          return sql::INIT_FAILURE;
        }
      }

      meta_table_.SetVersionNumber(35);
      meta_table_.SetCompatibleVersionNumber(
          std::min(35, kCompatibleVersionNumber));

      // FALL THROUGH

    // Add successive versions here.  Each should set the version number and
    // compatible version number as appropriate, then fall through to the next
    // case.

    case kCurrentVersionNumber:
      // No migration needed.
      return sql::INIT_OK;
  }
}
