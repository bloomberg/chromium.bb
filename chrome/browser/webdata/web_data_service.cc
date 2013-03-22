// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webdata/web_data_service.h"

#include "base/stl_util.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/webdata/autofill_change.h"
#include "chrome/browser/webdata/autofill_entry.h"
#include "chrome/browser/webdata/autofill_table.h"
#include "chrome/browser/webdata/keyword_table.h"
#include "chrome/browser/webdata/logins_table.h"
#include "chrome/browser/webdata/token_service_table.h"
#include "chrome/browser/webdata/web_apps_table.h"
#include "chrome/browser/webdata/web_database_service.h"
#include "chrome/browser/webdata/web_intents_table.h"
#include "chrome/common/chrome_notification_types.h"
#include "components/autofill/browser/autofill_country.h"
#include "components/autofill/browser/autofill_profile.h"
#include "components/autofill/browser/credit_card.h"
#include "components/autofill/common/form_field_data.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "third_party/skia/include/core/SkBitmap.h"

////////////////////////////////////////////////////////////////////////////////
//
// WebDataService implementation.
//
////////////////////////////////////////////////////////////////////////////////

using base::Bind;
using base::Time;
using content::BrowserThread;

namespace {

// A task used by WebDataService (for Sync mainly) to inform the
// PersonalDataManager living on the UI thread that it needs to refresh.
void NotifyOfMultipleAutofillChangesTask(
    const scoped_refptr<WebDataService>& web_data_service) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_AUTOFILL_MULTIPLE_CHANGED,
      content::Source<WebDataServiceBase>(web_data_service.get()),
      content::NotificationService::NoDetails());
}

}  // namespace

WDAppImagesResult::WDAppImagesResult() : has_all_images(false) {}

WDAppImagesResult::~WDAppImagesResult() {}

WDKeywordsResult::WDKeywordsResult()
  : default_search_provider_id(0),
    builtin_keyword_version(0) {
}

WDKeywordsResult::~WDKeywordsResult() {}

WebDataService::WebDataService(const base::FilePath& path,
                               const ProfileErrorCallback& callback)
    : WebDataServiceBase(path, callback) {
}

// static
void WebDataService::NotifyOfMultipleAutofillChanges(
    WebDataService* web_data_service) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));

  if (!web_data_service)
    return;

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      Bind(&NotifyOfMultipleAutofillChangesTask,
           make_scoped_refptr(web_data_service)));
}

//////////////////////////////////////////////////////////////////////////////
//
// Keywords.
//
//////////////////////////////////////////////////////////////////////////////

void WebDataService::AddKeyword(const TemplateURLData& data) {
  wdbs_->ScheduleDBTask(
      FROM_HERE, Bind(&WebDataService::AddKeywordImpl, this, data));
}

void WebDataService::RemoveKeyword(TemplateURLID id) {
  wdbs_->ScheduleDBTask(
      FROM_HERE, Bind(&WebDataService::RemoveKeywordImpl, this, id));
}

void WebDataService::UpdateKeyword(const TemplateURLData& data) {
  wdbs_->ScheduleDBTask(
      FROM_HERE, Bind(&WebDataService::UpdateKeywordImpl, this, data));
}

WebDataService::Handle WebDataService::GetKeywords(
    WebDataServiceConsumer* consumer) {
  return wdbs_->ScheduleDBTaskWithResult(FROM_HERE,
      Bind(&WebDataService::GetKeywordsImpl, this), consumer);
}

void WebDataService::SetDefaultSearchProvider(const TemplateURL* url) {
  wdbs_->ScheduleDBTask(FROM_HERE,
      Bind(&WebDataService::SetDefaultSearchProviderImpl, this,
           url ? url->id() : 0));
}

void WebDataService::SetBuiltinKeywordVersion(int version) {
  wdbs_->ScheduleDBTask(FROM_HERE,
      Bind(&WebDataService::SetBuiltinKeywordVersionImpl, this, version));
}

//////////////////////////////////////////////////////////////////////////////
//
// Web Apps
//
//////////////////////////////////////////////////////////////////////////////

void WebDataService::SetWebAppImage(const GURL& app_url,
                                    const SkBitmap& image) {
  wdbs_->ScheduleDBTask(FROM_HERE,
      Bind(&WebDataService::SetWebAppImageImpl, this, app_url, image));
}

void WebDataService::SetWebAppHasAllImages(const GURL& app_url,
                                           bool has_all_images) {
  wdbs_->ScheduleDBTask(FROM_HERE,
      Bind(&WebDataService::SetWebAppHasAllImagesImpl, this, app_url,
           has_all_images));
}

void WebDataService::RemoveWebApp(const GURL& app_url) {
  wdbs_->ScheduleDBTask(FROM_HERE,
      Bind(&WebDataService::RemoveWebAppImpl, this, app_url));
}

WebDataService::Handle WebDataService::GetWebAppImages(
    const GURL& app_url, WebDataServiceConsumer* consumer) {
  return wdbs_->ScheduleDBTaskWithResult(FROM_HERE,
      Bind(&WebDataService::GetWebAppImagesImpl, this, app_url), consumer);
}

////////////////////////////////////////////////////////////////////////////////
//
// Token Service
//
////////////////////////////////////////////////////////////////////////////////

void WebDataService::SetTokenForService(const std::string& service,
                                        const std::string& token) {
  wdbs_->ScheduleDBTask(FROM_HERE,
      Bind(&WebDataService::SetTokenForServiceImpl, this, service, token));
}

void WebDataService::RemoveAllTokens() {
  wdbs_->ScheduleDBTask(FROM_HERE,
      Bind(&WebDataService::RemoveAllTokensImpl, this));
}

// Null on failure. Success is WDResult<std::string>
WebDataService::Handle WebDataService::GetAllTokens(
    WebDataServiceConsumer* consumer) {
  return wdbs_->ScheduleDBTaskWithResult(FROM_HERE,
      Bind(&WebDataService::GetAllTokensImpl, this), consumer);
}

////////////////////////////////////////////////////////////////////////////////
//
// Autofill.
//
////////////////////////////////////////////////////////////////////////////////

void WebDataService::AddFormFields(
    const std::vector<FormFieldData>& fields) {
  wdbs_->ScheduleDBTask(FROM_HERE,
      Bind(&WebDataService::AddFormElementsImpl, this, fields));
}

WebDataService::Handle WebDataService::GetFormValuesForElementName(
    const string16& name, const string16& prefix, int limit,
    WebDataServiceConsumer* consumer) {
  return wdbs_->ScheduleDBTaskWithResult(FROM_HERE,
      Bind(&WebDataService::GetFormValuesForElementNameImpl,
           this, name, prefix, limit), consumer);
}

void WebDataService::RemoveFormElementsAddedBetween(const Time& delete_begin,
                                                    const Time& delete_end) {
  wdbs_->ScheduleDBTask(FROM_HERE,
      Bind(&WebDataService::RemoveFormElementsAddedBetweenImpl,
           this, delete_begin, delete_end));
}

void WebDataService::RemoveExpiredFormElements() {
  wdbs_->ScheduleDBTask(FROM_HERE,
      Bind(&WebDataService::RemoveExpiredFormElementsImpl, this));
}

void WebDataService::RemoveFormValueForElementName(
    const string16& name, const string16& value) {
  wdbs_->ScheduleDBTask(FROM_HERE,
      Bind(&WebDataService::RemoveFormValueForElementNameImpl,
           this, name, value));
}

void WebDataService::AddAutofillProfile(const AutofillProfile& profile) {
  wdbs_->ScheduleDBTask(FROM_HERE,
      Bind(&WebDataService::AddAutofillProfileImpl, this, profile));
}

void WebDataService::UpdateAutofillProfile(const AutofillProfile& profile) {
  wdbs_->ScheduleDBTask(FROM_HERE,
      Bind(&WebDataService::UpdateAutofillProfileImpl, this, profile));
}

void WebDataService::RemoveAutofillProfile(const std::string& guid) {
  wdbs_->ScheduleDBTask(FROM_HERE,
      Bind(&WebDataService::RemoveAutofillProfileImpl, this, guid));
}

WebDataService::Handle WebDataService::GetAutofillProfiles(
    WebDataServiceConsumer* consumer) {
  return wdbs_->ScheduleDBTaskWithResult(FROM_HERE,
      Bind(&WebDataService::GetAutofillProfilesImpl, this), consumer);
}

void WebDataService::AddCreditCard(const CreditCard& credit_card) {
  wdbs_->ScheduleDBTask(FROM_HERE,
      Bind(&WebDataService::AddCreditCardImpl, this, credit_card));
}

void WebDataService::UpdateCreditCard(const CreditCard& credit_card) {
  wdbs_->ScheduleDBTask(FROM_HERE,
      Bind(&WebDataService::UpdateCreditCardImpl, this, credit_card));
}

void WebDataService::RemoveCreditCard(const std::string& guid) {
  wdbs_->ScheduleDBTask(FROM_HERE,
      Bind(&WebDataService::RemoveCreditCardImpl, this, guid));
}

WebDataService::Handle WebDataService::GetCreditCards(
    WebDataServiceConsumer* consumer) {
  return wdbs_->ScheduleDBTaskWithResult(FROM_HERE,
      Bind(&WebDataService::GetCreditCardsImpl, this), consumer);
}

void WebDataService::RemoveAutofillProfilesAndCreditCardsModifiedBetween(
    const Time& delete_begin,
    const Time& delete_end) {
  wdbs_->ScheduleDBTask(FROM_HERE, Bind(
      &WebDataService::RemoveAutofillProfilesAndCreditCardsModifiedBetweenImpl,
      this, delete_begin, delete_end));
}

WebDataService::WebDataService()
    : WebDataServiceBase(base::FilePath(), ProfileErrorCallback()) {
}

WebDataService::~WebDataService() {
}

////////////////////////////////////////////////////////////////////////////////
//
// Keywords implementation.
//
////////////////////////////////////////////////////////////////////////////////

WebDatabase::State WebDataService::AddKeywordImpl(
    const TemplateURLData& data, WebDatabase* db) {
  KeywordTable::FromWebDatabase(db)->AddKeyword(data);
  return WebDatabase::COMMIT_NEEDED;
}

WebDatabase::State WebDataService::RemoveKeywordImpl(
    TemplateURLID id, WebDatabase* db) {
  DCHECK(id);
  KeywordTable::FromWebDatabase(db)->RemoveKeyword(id);
  return WebDatabase::COMMIT_NEEDED;
}

WebDatabase::State WebDataService::UpdateKeywordImpl(
    const TemplateURLData& data, WebDatabase* db) {
  if (!KeywordTable::FromWebDatabase(db)->UpdateKeyword(data)) {
    NOTREACHED();
    return WebDatabase::COMMIT_NOT_NEEDED;
  }
 return WebDatabase::COMMIT_NEEDED;
}

scoped_ptr<WDTypedResult> WebDataService::GetKeywordsImpl(WebDatabase* db) {
  WDKeywordsResult result;
  KeywordTable::FromWebDatabase(db)->GetKeywords(&result.keywords);
  result.default_search_provider_id =
      KeywordTable::FromWebDatabase(db)->GetDefaultSearchProviderID();
  result.builtin_keyword_version =
      KeywordTable::FromWebDatabase(db)->GetBuiltinKeywordVersion();
  return scoped_ptr<WDTypedResult>(
      new WDResult<WDKeywordsResult>(KEYWORDS_RESULT, result));
}

WebDatabase::State WebDataService::SetDefaultSearchProviderImpl(
    TemplateURLID id, WebDatabase* db) {
  if (!KeywordTable::FromWebDatabase(db)->SetDefaultSearchProviderID(id)) {
    NOTREACHED();
    return WebDatabase::COMMIT_NOT_NEEDED;
  }
  return WebDatabase::COMMIT_NEEDED;
}

WebDatabase::State WebDataService::SetBuiltinKeywordVersionImpl(
    int version, WebDatabase* db) {
  if (!KeywordTable::FromWebDatabase(db)->SetBuiltinKeywordVersion(version)) {
    NOTREACHED();
    return WebDatabase::COMMIT_NOT_NEEDED;
  }
  return WebDatabase::COMMIT_NEEDED;
}

////////////////////////////////////////////////////////////////////////////////
//
// Web Apps implementation.
//
////////////////////////////////////////////////////////////////////////////////

WebDatabase::State WebDataService::SetWebAppImageImpl(
    const GURL& app_url, const SkBitmap& image, WebDatabase* db) {
  WebAppsTable::FromWebDatabase(db)->SetWebAppImage(app_url, image);
  return WebDatabase::COMMIT_NEEDED;
}

WebDatabase::State WebDataService::SetWebAppHasAllImagesImpl(
    const GURL& app_url, bool has_all_images, WebDatabase* db) {
  WebAppsTable::FromWebDatabase(db)->SetWebAppHasAllImages(app_url,
                                                           has_all_images);
  return WebDatabase::COMMIT_NEEDED;
}

WebDatabase::State WebDataService::RemoveWebAppImpl(
    const GURL& app_url, WebDatabase* db) {
  WebAppsTable::FromWebDatabase(db)->RemoveWebApp(app_url);
  return WebDatabase::COMMIT_NEEDED;
}

scoped_ptr<WDTypedResult> WebDataService::GetWebAppImagesImpl(
    const GURL& app_url, WebDatabase* db) {
  WDAppImagesResult result;
  result.has_all_images =
      WebAppsTable::FromWebDatabase(db)->GetWebAppHasAllImages(app_url);
  WebAppsTable::FromWebDatabase(db)->GetWebAppImages(app_url, &result.images);
  return scoped_ptr<WDTypedResult>(
      new WDResult<WDAppImagesResult>(WEB_APP_IMAGES, result));
}

////////////////////////////////////////////////////////////////////////////////
//
// Token Service implementation.
//
////////////////////////////////////////////////////////////////////////////////

WebDatabase::State WebDataService::RemoveAllTokensImpl(WebDatabase* db) {
  if (TokenServiceTable::FromWebDatabase(db)->RemoveAllTokens()) {
    return WebDatabase::COMMIT_NEEDED;
  }
  return WebDatabase::COMMIT_NOT_NEEDED;
}

WebDatabase::State WebDataService::SetTokenForServiceImpl(
    const std::string& service, const std::string& token, WebDatabase* db) {
  if (TokenServiceTable::FromWebDatabase(db)->SetTokenForService(service,
                                                                 token)) {
    return WebDatabase::COMMIT_NEEDED;
  }
  return WebDatabase::COMMIT_NOT_NEEDED;
}

scoped_ptr<WDTypedResult> WebDataService::GetAllTokensImpl(WebDatabase* db) {
  std::map<std::string, std::string> map;
  TokenServiceTable::FromWebDatabase(db)->GetAllTokens(&map);
  return scoped_ptr<WDTypedResult>(
      new WDResult<std::map<std::string, std::string> >(TOKEN_RESULT, map));
}

////////////////////////////////////////////////////////////////////////////////
//
// Autofill implementation.
//
////////////////////////////////////////////////////////////////////////////////

WebDatabase::State WebDataService::AddFormElementsImpl(
    const std::vector<FormFieldData>& fields, WebDatabase* db) {
  AutofillChangeList changes;
  if (!AutofillTable::FromWebDatabase(db)->AddFormFieldValues(
          fields, &changes)) {
    NOTREACHED();
    return WebDatabase::COMMIT_NOT_NEEDED;
  }

  // Post the notifications including the list of affected keys.
  // This is sent here so that work resulting from this notification will be
  // done on the DB thread, and not the UI thread.
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_AUTOFILL_ENTRIES_CHANGED,
      content::Source<WebDataService>(this),
      content::Details<AutofillChangeList>(&changes));

  return WebDatabase::COMMIT_NEEDED;
}

scoped_ptr<WDTypedResult> WebDataService::GetFormValuesForElementNameImpl(
    const string16& name, const string16& prefix, int limit, WebDatabase* db) {
  std::vector<string16> values;
  AutofillTable::FromWebDatabase(db)->GetFormValuesForElementName(
      name, prefix, &values, limit);
  return scoped_ptr<WDTypedResult>(
      new WDResult<std::vector<string16> >(AUTOFILL_VALUE_RESULT, values));
}

WebDatabase::State WebDataService::RemoveFormElementsAddedBetweenImpl(
    const base::Time& delete_begin, const base::Time& delete_end,
    WebDatabase* db) {
  AutofillChangeList changes;

  if (AutofillTable::FromWebDatabase(db)->RemoveFormElementsAddedBetween(
      delete_begin, delete_end, &changes)) {
    if (!changes.empty()) {
      // Post the notifications including the list of affected keys.
      // This is sent here so that work resulting from this notification
      // will be done on the DB thread, and not the UI thread.
      content::NotificationService::current()->Notify(
          chrome::NOTIFICATION_AUTOFILL_ENTRIES_CHANGED,
          content::Source<WebDataService>(this),
          content::Details<AutofillChangeList>(&changes));
    }
    return WebDatabase::COMMIT_NEEDED;
  }
  return WebDatabase::COMMIT_NOT_NEEDED;
}

WebDatabase::State WebDataService::RemoveExpiredFormElementsImpl(
    WebDatabase* db) {
  AutofillChangeList changes;

  if (AutofillTable::FromWebDatabase(db)->RemoveExpiredFormElements(&changes)) {
    if (!changes.empty()) {
      // Post the notifications including the list of affected keys.
      // This is sent here so that work resulting from this notification
      // will be done on the DB thread, and not the UI thread.
      content::NotificationService::current()->Notify(
          chrome::NOTIFICATION_AUTOFILL_ENTRIES_CHANGED,
          content::Source<WebDataService>(this),
          content::Details<AutofillChangeList>(&changes));
    }
    return WebDatabase::COMMIT_NEEDED;
  }
  return WebDatabase::COMMIT_NOT_NEEDED;
}

WebDatabase::State WebDataService::RemoveFormValueForElementNameImpl(
    const string16& name, const string16& value, WebDatabase* db) {

  if (AutofillTable::FromWebDatabase(db)->RemoveFormElement(name, value)) {
    AutofillChangeList changes;
    changes.push_back(AutofillChange(AutofillChange::REMOVE,
                                     AutofillKey(name, value)));

    // Post the notifications including the list of affected keys.
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_AUTOFILL_ENTRIES_CHANGED,
        content::Source<WebDataService>(this),
        content::Details<AutofillChangeList>(&changes));

    return WebDatabase::COMMIT_NEEDED;
  }
  return WebDatabase::COMMIT_NOT_NEEDED;
}

WebDatabase::State WebDataService::AddAutofillProfileImpl(
    const AutofillProfile& profile, WebDatabase* db) {
  if (!AutofillTable::FromWebDatabase(db)->AddAutofillProfile(profile)) {
    NOTREACHED();
    return WebDatabase::COMMIT_NOT_NEEDED;
  }

  // Send GUID-based notification.
  AutofillProfileChange change(AutofillProfileChange::ADD,
                               profile.guid(), &profile);
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_AUTOFILL_PROFILE_CHANGED,
      content::Source<WebDataService>(this),
      content::Details<AutofillProfileChange>(&change));

  return WebDatabase::COMMIT_NEEDED;
}

WebDatabase::State WebDataService::UpdateAutofillProfileImpl(
    const AutofillProfile& profile, WebDatabase* db) {
  // Only perform the update if the profile exists.  It is currently
  // valid to try to update a missing profile.  We simply drop the write and
  // the caller will detect this on the next refresh.
  AutofillProfile* original_profile = NULL;
  if (!AutofillTable::FromWebDatabase(db)->GetAutofillProfile(profile.guid(),
      &original_profile)) {
    return WebDatabase::COMMIT_NOT_NEEDED;
  }
  scoped_ptr<AutofillProfile> scoped_profile(original_profile);

  if (!AutofillTable::FromWebDatabase(db)->UpdateAutofillProfileMulti(
          profile)) {
    NOTREACHED();
    return WebDatabase::COMMIT_NEEDED;
  }

  // Send GUID-based notification.
  AutofillProfileChange change(AutofillProfileChange::UPDATE,
                               profile.guid(), &profile);
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_AUTOFILL_PROFILE_CHANGED,
      content::Source<WebDataService>(this),
      content::Details<AutofillProfileChange>(&change));

  return WebDatabase::COMMIT_NEEDED;
}

WebDatabase::State WebDataService::RemoveAutofillProfileImpl(
    const std::string& guid, WebDatabase* db) {
  AutofillProfile* profile = NULL;
  if (!AutofillTable::FromWebDatabase(db)->GetAutofillProfile(guid, &profile)) {
    NOTREACHED();
    return WebDatabase::COMMIT_NOT_NEEDED;
  }
  scoped_ptr<AutofillProfile> scoped_profile(profile);

  if (!AutofillTable::FromWebDatabase(db)->RemoveAutofillProfile(guid)) {
    NOTREACHED();
    return WebDatabase::COMMIT_NOT_NEEDED;
  }

  // Send GUID-based notification.
  AutofillProfileChange change(AutofillProfileChange::REMOVE, guid, NULL);
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_AUTOFILL_PROFILE_CHANGED,
      content::Source<WebDataService>(this),
      content::Details<AutofillProfileChange>(&change));

  return WebDatabase::COMMIT_NEEDED;
}

scoped_ptr<WDTypedResult> WebDataService::GetAutofillProfilesImpl(
    WebDatabase* db) {
  std::vector<AutofillProfile*> profiles;
  AutofillTable::FromWebDatabase(db)->GetAutofillProfiles(&profiles);
  return scoped_ptr<WDTypedResult>(
      new WDDestroyableResult<std::vector<AutofillProfile*> >(
          AUTOFILL_PROFILES_RESULT,
          profiles,
          base::Bind(&WebDataService::DestroyAutofillProfileResult,
              base::Unretained(this))));
}

WebDatabase::State WebDataService::AddCreditCardImpl(
    const CreditCard& credit_card, WebDatabase* db) {
  if (!AutofillTable::FromWebDatabase(db)->AddCreditCard(credit_card)) {
    NOTREACHED();
    return WebDatabase::COMMIT_NOT_NEEDED;
  }

  return WebDatabase::COMMIT_NEEDED;
}

WebDatabase::State WebDataService::UpdateCreditCardImpl(
    const CreditCard& credit_card, WebDatabase* db) {
  // It is currently valid to try to update a missing profile.  We simply drop
  // the write and the caller will detect this on the next refresh.
  CreditCard* original_credit_card = NULL;
  if (!AutofillTable::FromWebDatabase(db)->GetCreditCard(credit_card.guid(),
      &original_credit_card)) {
    return WebDatabase::COMMIT_NOT_NEEDED;
  }
  scoped_ptr<CreditCard> scoped_credit_card(original_credit_card);

  if (!AutofillTable::FromWebDatabase(db)->UpdateCreditCard(credit_card)) {
    NOTREACHED();
    return WebDatabase::COMMIT_NOT_NEEDED;
  }
  return WebDatabase::COMMIT_NEEDED;
}

WebDatabase::State WebDataService::RemoveCreditCardImpl(
    const std::string& guid, WebDatabase* db) {
  if (!AutofillTable::FromWebDatabase(db)->RemoveCreditCard(guid)) {
    NOTREACHED();
    return WebDatabase::COMMIT_NOT_NEEDED;
  }
  return WebDatabase::COMMIT_NEEDED;
}

scoped_ptr<WDTypedResult> WebDataService::GetCreditCardsImpl(WebDatabase* db) {
  std::vector<CreditCard*> credit_cards;
  AutofillTable::FromWebDatabase(db)->GetCreditCards(&credit_cards);
  return scoped_ptr<WDTypedResult>(
      new WDDestroyableResult<std::vector<CreditCard*> >(
          AUTOFILL_CREDITCARDS_RESULT,
          credit_cards,
          base::Bind(&WebDataService::DestroyAutofillCreditCardResult,
              base::Unretained(this))));
}

WebDatabase::State
WebDataService::RemoveAutofillProfilesAndCreditCardsModifiedBetweenImpl(
        const base::Time& delete_begin, const base::Time& delete_end,
        WebDatabase* db) {
  std::vector<std::string> profile_guids;
  std::vector<std::string> credit_card_guids;
  if (AutofillTable::FromWebDatabase(db)->
      RemoveAutofillProfilesAndCreditCardsModifiedBetween(
          delete_begin,
          delete_end,
          &profile_guids,
          &credit_card_guids)) {
    for (std::vector<std::string>::iterator iter = profile_guids.begin();
         iter != profile_guids.end(); ++iter) {
      AutofillProfileChange change(AutofillProfileChange::REMOVE, *iter,
                                   NULL);
      content::NotificationService::current()->Notify(
          chrome::NOTIFICATION_AUTOFILL_PROFILE_CHANGED,
          content::Source<WebDataService>(this),
          content::Details<AutofillProfileChange>(&change));
    }
    // Note: It is the caller's responsibility to post notifications for any
    // changes, e.g. by calling the Refresh() method of PersonalDataManager.
    return WebDatabase::COMMIT_NEEDED;
  }
  return WebDatabase::COMMIT_NOT_NEEDED;
}

void WebDataService::DestroyAutofillProfileResult(const WDTypedResult* result) {
  DCHECK(result->GetType() == AUTOFILL_PROFILES_RESULT);
  const WDResult<std::vector<AutofillProfile*> >* r =
      static_cast<const WDResult<std::vector<AutofillProfile*> >*>(result);
  std::vector<AutofillProfile*> profiles = r->GetValue();
  STLDeleteElements(&profiles);
}

void WebDataService::DestroyAutofillCreditCardResult(
      const WDTypedResult* result) {
  DCHECK(result->GetType() == AUTOFILL_CREDITCARDS_RESULT);
  const WDResult<std::vector<CreditCard*> >* r =
      static_cast<const WDResult<std::vector<CreditCard*> >*>(result);

  std::vector<CreditCard*> credit_cards = r->GetValue();
  STLDeleteElements(&credit_cards);
}
