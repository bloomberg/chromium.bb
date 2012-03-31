// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/android/android_provider_backend.h"

#include "base/i18n/case_conversion.h"
#include "chrome/browser/bookmarks/bookmark_service.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/browser/history/android/android_time.h"
#include "chrome/browser/history/android/android_urls_sql_handler.h"
#include "chrome/browser/history/android/bookmark_model_sql_handler.h"
#include "chrome/browser/history/android/favicon_sql_handler.h"
#include "chrome/browser/history/android/urls_sql_handler.h"
#include "chrome/browser/history/android/visit_sql_handler.h"
#include "chrome/browser/history/history_backend.h"
#include "chrome/browser/history/history_database.h"
#include "chrome/browser/history/thumbnail_database.h"
#include "content/public/common/page_transition_types.h"
#include "sql/connection.h"

using base::Time;
using base::TimeDelta;

namespace history {

namespace {

const char* kVirtualBookmarkTable =
    "SELECT android_urls.id AS _id, "
        "android_cache_db.bookmark_cache.created_time AS created, "
        "urls.title AS title, android_urls.raw_url AS url, "
        "urls.visit_count AS visits, "
        "android_cache_db.bookmark_cache.last_visit_time AS date, "
        "android_cache_db.bookmark_cache.bookmark AS bookmark, "
        "android_cache_db.bookmark_cache.favicon_id AS favicon, "
        "urls.id AS url_id, urls.url AS urls_url "
    "FROM (android_urls JOIN urls on (android_urls.url_id = urls.id) "
        "LEFT JOIN android_cache_db.bookmark_cache AS bookmark_cache "
        "on (android_urls.url_id = bookmark_cache.url_id))";

const char * kURLUpdateClause =
    "SELECT urls.id, urls.last_visit_time, created_time, urls.url "
    "FROM urls LEFT JOIN "
        "(SELECT url as visit_url, min(visit_time) as created_time"
        " FROM visits GROUP BY url) ON (visit_url = urls.id) ";

const char* kSearchTermUpdateClause =
    "SELECT keyword_search_terms.term, max(urls.last_visit_time) "
    "FROM keyword_search_terms JOIN urls ON "
        "(keyword_search_terms.url_id = urls.id) "
    "GROUP BY keyword_search_terms.term";

void BindStatement(const std::vector<string16>& selection_args,
                   sql::Statement* statement,
                   int* col_index) {
  for (std::vector<string16>::const_iterator i = selection_args.begin();
       i != selection_args.end(); ++i) {
    // Using the same method as Android, binding all argument as String.
    statement->BindString16(*col_index, *i);
    (*col_index)++;
  }
}

bool IsBookmarkRowValid(const BookmarkRow& row) {
  // The caller should make sure both/neither Raw URL and/nor URL should be set.
  DCHECK(row.is_value_set_explicitly(BookmarkRow::RAW_URL) ==
         row.is_value_set_explicitly(BookmarkRow::URL));

  // The following cases are checked:
  // a. Last visit time or created time is large than now.
  // b. Last visit time is less than created time.
  // c. Created time and last visit time is different, but visit count is less
  //    than 2.
  // d. The difference between created and last visit time is less than
  //    visit_count.
  // e. Visit count is 0, but any one of last visit time and created time is
  //    set and not equal to 0.
  if (row.is_value_set_explicitly(BookmarkRow::LAST_VISIT_TIME) &&
      row.last_visit_time() > Time::Now())
    return false;

  if (row.is_value_set_explicitly(BookmarkRow::CREATED) &&
      row.created() > Time::Now())
    return false;

  if (row.is_value_set_explicitly(BookmarkRow::LAST_VISIT_TIME) &&
      row.is_value_set_explicitly(BookmarkRow::CREATED)) {
    if (row.created() > row.last_visit_time())
      return false;

    if (row.is_value_set_explicitly(BookmarkRow::VISIT_COUNT)) {
      if ((row.created() != row.last_visit_time() &&
           row.visit_count() < 2) ||
          (row.last_visit_time().ToInternalValue() -
           row.created().ToInternalValue() < row.visit_count()))
      return false;
    }
  }

  if (row.is_value_set_explicitly(BookmarkRow::VISIT_COUNT) &&
      row.visit_count() == 0 &&
      ((row.is_value_set_explicitly(BookmarkRow::CREATED) &&
        row.created() != Time()) ||
       (row.is_value_set_explicitly(BookmarkRow::LAST_VISIT_TIME) &&
        row.last_visit_time() != Time())))
    return false;

  return true;
}

}  // namespace

AndroidProviderBackend::AndroidProviderBackend(
    const FilePath& db_name,
    HistoryDatabase* history_db,
    ThumbnailDatabase* thumbnail_db,
    BookmarkService* bookmark_service,
    HistoryBackend::Delegate* delegate)
    : android_cache_db_filename_(db_name),
      db_(&history_db->GetDB()),
      history_db_(history_db),
      thumbnail_db_(thumbnail_db),
      bookmark_service_(bookmark_service),
      initialized_(false),
      delegate_(delegate) {
  DCHECK(delegate_);
}

AndroidProviderBackend::~AndroidProviderBackend() {
}

AndroidStatement* AndroidProviderBackend::QueryBookmarks(
    const std::vector<BookmarkRow::BookmarkColumnID>& projections,
    const std::string& selection,
    const std::vector<string16>& selection_args,
    const std::string& sort_order) {
  if (projections.empty())
    return NULL;

  ScopedTransaction transaction(history_db_, thumbnail_db_);

  if (!EnsureInitializedAndUpdated())
    return NULL;

  transaction.Commit();

  return QueryBookmarksInternal(projections, selection, selection_args,
                                sort_order);
}

bool AndroidProviderBackend::UpdateBookmarks(
    const BookmarkRow& row,
    const std::string& selection,
    const std::vector<string16>& selection_args,
    int* updated_count) {
  HistoryNotifications notifications;

  ScopedTransaction transaction(history_db_, thumbnail_db_);

  if (!UpdateBookmarks(row, selection, selection_args, updated_count,
                       &notifications))
    return false;

  transaction.Commit();
  BroadcastNotifications(notifications);
  return true;
}

AndroidURLID AndroidProviderBackend::InsertBookmark(const BookmarkRow& values) {
  HistoryNotifications notifications;

  ScopedTransaction transaction(history_db_, thumbnail_db_);

  AndroidURLID id = InsertBookmark(values, &notifications);
  if (id) {
    transaction.Commit();
    BroadcastNotifications(notifications);
    return id;
  }
  return 0;
}

bool AndroidProviderBackend::DeleteBookmarks(
    const std::string& selection,
    const std::vector<string16>& selection_args,
    int* deleted_count) {
  HistoryNotifications notifications;

  ScopedTransaction transaction(history_db_, thumbnail_db_);

  if (!DeleteBookmarks(selection, selection_args, deleted_count,
                       &notifications))
    return false;

  transaction.Commit();
  BroadcastNotifications(notifications);
  return true;
}


AndroidProviderBackend::HistoryNotification::HistoryNotification(
    int type,
    HistoryDetails* detail)
    : type(type),
      detail(detail) {
}

AndroidProviderBackend::HistoryNotification::~HistoryNotification() {
}

AndroidProviderBackend::ScopedTransaction::ScopedTransaction(
    HistoryDatabase* history_db,
    ThumbnailDatabase* thumbnail_db)
    : history_db_(history_db),
      thumbnail_db_(thumbnail_db),
      committed_(false) {
  history_db_->BeginTransaction();
  thumbnail_db_->BeginTransaction();
}

AndroidProviderBackend::ScopedTransaction::~ScopedTransaction() {
  if (!committed_) {
    history_db_->RollbackTransaction();
    thumbnail_db_->RollbackTransaction();
  }
}

void AndroidProviderBackend::ScopedTransaction::Commit() {
  DCHECK(!committed_);
  history_db_->CommitTransaction();
  thumbnail_db_->CommitTransaction();
  committed_ = true;
}

bool AndroidProviderBackend::UpdateBookmarks(
    const BookmarkRow& row,
    const std::string& selection,
    const std::vector<string16>& selection_args,
    int* updated_count,
    HistoryNotifications* notifications) {
  if (!IsBookmarkRowValid(row))
    return false;

  if (row.is_value_set_explicitly(BookmarkRow::ID))
    return false;

  if (!EnsureInitializedAndUpdated())
    return false;

  TableIDRows ids_set;
  if (!GetSelectedURLs(selection, selection_args, kVirtualBookmarkTable,
                       &ids_set))
    return false;

  if (ids_set.empty()) {
    *updated_count = 0;
    return true;
  }

  // URL can not be updated, we simulate the update.
  if (row.is_value_set_explicitly(BookmarkRow::URL)) {
    // Only one row's URL can be updated at a time as we can't have multiple
    // rows have the same URL.
    if (ids_set.size() != 1)
      return false;

    BookmarkRow new_row = row;
    if (!SimulateUpdateURL(new_row, ids_set, notifications))
      return false;
    *updated_count = 1;
    return true;
  }

  for (std::vector<SQLHandler*>::iterator i =
       sql_handlers_.begin(); i != sql_handlers_.end(); ++i) {
    if ((*i)->HasColumnIn(row)) {
      if (!(*i)->Update(row, ids_set))
        return false;
    }
  }
  *updated_count = ids_set.size();

  scoped_ptr<URLsModifiedDetails> modified(new URLsModifiedDetails);
  scoped_ptr<FaviconChangeDetails> favicon(new FaviconChangeDetails);

  for (TableIDRows::const_iterator i = ids_set.begin(); i != ids_set.end();
       ++i) {
    if (row.is_value_set_explicitly(BookmarkRow::TITLE) ||
        row.is_value_set_explicitly(BookmarkRow::VISIT_COUNT) ||
        row.is_value_set_explicitly(BookmarkRow::LAST_VISIT_TIME)) {
      URLRow url_row;
      if (!history_db_->GetURLRow(i->url_id, &url_row))
        return false;
      modified->changed_urls.push_back(url_row);
    }
    if (row.is_value_set_explicitly(BookmarkRow::FAVICON))
      favicon->urls.insert(i->url);
  }

  if (!modified->changed_urls.empty())
    notifications->push_back(HistoryNotification(
        chrome::NOTIFICATION_HISTORY_TYPED_URLS_MODIFIED, modified.release()));

  if (!favicon->urls.empty())
    notifications->push_back(HistoryNotification(
        chrome::NOTIFICATION_FAVICON_CHANGED, favicon.release()));

  return true;
}

AndroidURLID AndroidProviderBackend::InsertBookmark(
    const BookmarkRow& values,
    HistoryNotifications* notifications) {
  if (!IsBookmarkRowValid(values))
    return false;

  if (!EnsureInitializedAndUpdated())
    return 0;

  DCHECK(values.is_value_set_explicitly(BookmarkRow::URL));
  // Make a copy of values as we need change it during insert.
  BookmarkRow row = values;
  for (std::vector<SQLHandler*>::iterator i =
       sql_handlers_.begin(); i != sql_handlers_.end(); ++i) {
    if (!(*i)->Insert(&row))
      return 0;
  }

  URLRow url_row;
  if (!history_db_->GetURLRow(row.url_id(), &url_row))
    return false;

  scoped_ptr<URLsModifiedDetails> modified(new URLsModifiedDetails);
  if (!modified.get())
    return false;
  modified->changed_urls.push_back(url_row);

  scoped_ptr<FaviconChangeDetails> favicon;
  if (row.is_value_set_explicitly(BookmarkRow::FAVICON) &&
      !row.favicon().empty()) {
    favicon.reset(new FaviconChangeDetails);
    if (!favicon.get())
      return false;
    favicon->urls.insert(url_row.url());
  }

  notifications->push_back(HistoryNotification(
      chrome::NOTIFICATION_HISTORY_TYPED_URLS_MODIFIED, modified.release()));
  if (favicon.get())
    notifications->push_back(HistoryNotification(
        chrome::NOTIFICATION_FAVICON_CHANGED, favicon.release()));

  return row.id();
}

bool AndroidProviderBackend::DeleteBookmarks(
    const std::string& selection,
    const std::vector<string16>& selection_args,
    int * deleted_count,
    HistoryNotifications* notifications) {
  if (!EnsureInitializedAndUpdated())
    return false;

  TableIDRows ids_set;
  if (!GetSelectedURLs(selection, selection_args, kVirtualBookmarkTable,
                       &ids_set))
    return false;

  if (ids_set.empty()) {
    deleted_count = 0;
    return true;
  }

  scoped_ptr<URLsDeletedDetails> deleted_details(new URLsDeletedDetails);
  scoped_ptr<FaviconChangeDetails> favicon(new FaviconChangeDetails);
  for (TableIDRows::const_iterator i = ids_set.begin(); i != ids_set.end();
       ++i) {
    URLRow url_row;
    if (!history_db_->GetURLRow(i->url_id, &url_row))
      return false;
    deleted_details->rows.push_back(url_row);
    deleted_details->urls.insert(url_row.url());
    if (thumbnail_db_->GetIconMappingsForPageURL(url_row.url(), NULL))
      favicon->urls.insert(url_row.url());
  }

  for (std::vector<SQLHandler*>::iterator i =
       sql_handlers_.begin(); i != sql_handlers_.end(); ++i) {
    if (!(*i)->Delete(ids_set))
      return false;
  }
  *deleted_count = ids_set.size();
  notifications->push_back(HistoryNotification(
      chrome::NOTIFICATION_HISTORY_URLS_DELETED,
      deleted_details.release()));
  if (favicon.get() && !favicon->urls.empty())
    notifications->push_back(HistoryNotification(
        chrome::NOTIFICATION_FAVICON_CHANGED, favicon.release()));

  return true;
}

AndroidStatement* AndroidProviderBackend::QuerySearchTerms(
    const std::vector<SearchRow::SearchColumnID>& projections,
    const std::string& selection,
    const std::vector<string16>& selection_args,
    const std::string& sort_order) {
  if (projections.empty())
    return NULL;

  if (!EnsureInitializedAndUpdated())
    return NULL;

  std::string sql;
  sql.append("SELECT ");
  AppendSearchResultColumn(projections, &sql);
  sql.append(" FROM android_cache_db.search_terms ");

  if (!selection.empty()) {
    sql.append(" WHERE ");
    sql.append(selection);
  }

  if (!sort_order.empty()) {
    sql.append(" ORDER BY ");
    sql.append(sort_order);
  }

  scoped_ptr<sql::Statement> statement(new sql::Statement(
      db_->GetUniqueStatement(sql.c_str())));
  int count = 0;
  BindStatement(selection_args, statement.get(), &count);
  if (!statement->is_valid()) {
    LOG(ERROR) << db_->GetErrorMessage();
    return NULL;
  }
  sql::Statement* result = statement.release();
  return new AndroidStatement(result, -1);
}

bool AndroidProviderBackend::UpdateSearchTerms(
    const SearchRow& row,
    const std::string& selection,
    const std::vector<string16>& selection_args,
    int* update_count) {
  if (!EnsureInitializedAndUpdated())
    return false;

  SearchTerms search_terms;
  if (!GetSelectedSearchTerms(selection, selection_args, &search_terms))
    return false;

  // We can not update search term if multiple row selected.
  if (row.is_value_set_explicitly(SearchRow::SEARCH_TERM) &&
      search_terms.size() > 1)
    return false;

  *update_count = search_terms.size();

  if (search_terms.empty())
    return true;

  if (row.is_value_set_explicitly(SearchRow::SEARCH_TERM)) {
    SearchTermRow search_term_row;
    SearchRow search_row = row;
    if (!history_db_->GetSearchTerm(search_terms[0], &search_term_row))
      return false;

    search_term_row.term = search_row.search_term();
    if (!search_row.is_value_set_explicitly(SearchRow::SEARCH_TIME))
      search_row.set_search_time(search_term_row.last_visit_time);
    else
      search_term_row.last_visit_time = search_row.search_time();

    // Delete the original search term.
    if (!history_db_->DeleteKeywordSearchTerm(search_terms[0]))
      return false;

    // Add the new one.
    if (!AddSearchTerm(search_row))
      return false;

    // Update the cache table so the id will not be changed.
    if (!history_db_->UpdateSearchTerm(search_term_row.id, search_term_row))
      return false;

     return true;
  }

  for (SearchTerms::const_iterator i = search_terms.begin();
       i != search_terms.end(); ++i) {
    SearchTermRow search_term_row;
    if (!history_db_->GetSearchTerm(*i, &search_term_row))
      return false;

    // Check whether the given search time less than the existing one.
    if (search_term_row.last_visit_time > row.search_time())
      return false;

    std::vector<KeywordSearchTermRow> search_term_rows;
    if (!history_db_->GetKeywordSearchTermRows(*i, &search_term_rows) ||
        search_term_rows.empty())
      return false;

    // Actually only search_time update. As there might multiple URLs
    // asocciated with the keyword, Just update the first one's last_visit_time.
    URLRow url_row;
    if (!history_db_->GetURLRow(search_term_rows[0].url_id, &url_row))
      return false;

    BookmarkRow bookmark_row;
    bookmark_row.set_last_visit_time(row.search_time());
    TableIDRow table_id_row;
    table_id_row.url_id = url_row.id();
    TableIDRows table_id_rows;
    table_id_rows.push_back(table_id_row);
    if (!urls_handler_->Update(bookmark_row, table_id_rows))
      return false;

    if (!visit_handler_->Update(bookmark_row, table_id_rows))
      return false;
  }
  return true;
}

SearchTermID AndroidProviderBackend::InsertSearchTerm(
    const SearchRow& values) {
  if (!EnsureInitializedAndUpdated())
    return 0;

  if (!AddSearchTerm(values))
    return 0;

  SearchTermID id = history_db_->GetSearchTerm(values.search_term(), NULL);
  if (!id)
    // Note the passed in Time() will be changed in UpdateSearchTermTable().
    id = history_db_->AddSearchTerm(values.search_term(), Time());
  return id;
}

bool AndroidProviderBackend::DeleteSearchTerms(
    const std::string& selection,
    const std::vector<string16>& selection_args,
    int * deleted_count) {
  SearchTerms rows;
  if (!GetSelectedSearchTerms(selection, selection_args, &rows))
    return false;

  *deleted_count = rows.size();
  if (rows.empty())
    return true;

  for (SearchTerms::const_iterator i = rows.begin(); i != rows.end(); ++i)
    if (!history_db_->DeleteKeywordSearchTerm(*i))
      return false;
  // We don't delete the rows in search_terms table, as once the
  // search_terms table is updated with keyword_search_terms, all
  // keyword cache not found in the keyword_search_terms will be removed.
  return true;
}

bool AndroidProviderBackend::EnsureInitializedAndUpdated() {
  if (!initialized_) {
    if (!Init())
      return false;
  }
  return UpdateTables();
}

bool AndroidProviderBackend::Init() {
  urls_handler_.reset(new UrlsSQLHandler(history_db_));
  visit_handler_.reset(new VisitSQLHandler(history_db_));
  android_urls_handler_.reset(new AndroidURLsSQLHandler(history_db_));
  favicon_handler_.reset(new FaviconSQLHandler(thumbnail_db_));
  bookmark_model_handler_.reset(new BookmarkModelSQLHandler(history_db_));
  // The urls_handler must be pushed first, because the subsequent handlers
  // depend on its output.
  sql_handlers_.push_back(urls_handler_.get());
  sql_handlers_.push_back(visit_handler_.get());
  sql_handlers_.push_back(android_urls_handler_.get());
  sql_handlers_.push_back(favicon_handler_.get());
  sql_handlers_.push_back(bookmark_model_handler_.get());

  if (!history_db_->CreateAndroidURLsTable())
    return false;
  if (sql::INIT_OK != history_db_->InitAndroidCacheDatabase(
          android_cache_db_filename_))
    return false;
  initialized_ = true;
  return true;
}

bool AndroidProviderBackend::UpdateTables() {
  if (!UpdateVisitedURLs()) {
    LOG(ERROR) << "Update of the visisted URLS failed";
    return false;
  }

  if (!UpdateRemovedURLs()) {
    LOG(ERROR) << "Update of the removed URLS failed";
    return false;
  }

  if (!UpdateBookmarks()) {
    LOG(ERROR) << "Update of the bookmarks failed";
    return false;
  }

  if (!UpdateFavicon()) {
    LOG(ERROR) << "Update of the icons failed";
    return false;
  }

  if (!UpdateSearchTermTable()) {
    LOG(ERROR) << "Update of the search_terms failed";
    return false;
  }
  return true;
}

bool AndroidProviderBackend::UpdateVisitedURLs() {
  std::string sql(kURLUpdateClause);
  sql.append("WHERE urls.id NOT IN (SELECT url_id FROM android_urls)");
  sql::Statement urls_statement(db_->GetCachedStatement(SQL_FROM_HERE,
                                                        sql.c_str()));
  if (!urls_statement.is_valid()) {
    LOG(ERROR) << db_->GetErrorMessage();
    return false;
  }

  while (urls_statement.Step()) {
    if (history_db_->GetAndroidURLRow(urls_statement.ColumnInt64(0), NULL))
      continue;
    if (!history_db_->AddAndroidURLRow(urls_statement.ColumnString(3),
                                       urls_statement.ColumnInt64(0)))
      return false;
  }

  if (!history_db_->ClearAllBookmarkCache())
    return false;

  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE,
                                                   kURLUpdateClause));
  while (statement.Step()) {
    if (!history_db_->AddBookmarkCacheRow(
            Time::FromInternalValue(statement.ColumnInt64(2)),
            Time::FromInternalValue(statement.ColumnInt64(1)),
            statement.ColumnInt64(0)))
      return false;
  }
  return true;
}

bool AndroidProviderBackend::UpdateRemovedURLs() {
  return history_db_->DeleteUnusedAndroidURLs();
}

bool AndroidProviderBackend::UpdateBookmarks() {
  if (bookmark_service_ == NULL) {
    LOG(ERROR) << "Bookmark service is not available";
    return false;
  }

  bookmark_service_->BlockTillLoaded();
  std::vector<GURL> bookmark_urls;
  bookmark_service_->GetBookmarks(&bookmark_urls);

  if (bookmark_urls.empty())
    return true;

  std::vector<URLID> url_ids;
  for (std::vector<GURL>::const_iterator i = bookmark_urls.begin();
      i != bookmark_urls.end(); ++i) {
    URLID url_id = history_db_->GetRowForURL(*i, NULL);
    if (url_id == 0) {
      VLOG(1) << "Can not find bookmark in history " << i->spec();
      continue;
    }
    url_ids.push_back(url_id);
  }

  return history_db_->MarkURLsAsBookmarked(url_ids);
}

bool AndroidProviderBackend::UpdateFavicon() {
  ThumbnailDatabase::IconMappingEnumerator enumerator;
  if (!thumbnail_db_->InitIconMappingEnumerator(FAVICON, &enumerator))
    return false;

  IconMapping icon_mapping;
  while (enumerator.GetNextIconMapping(&icon_mapping)) {
    URLID url_id = history_db_->GetRowForURL(icon_mapping.page_url, NULL);
    if (url_id == 0) {
      LOG(ERROR) << "Can not find favicon's page url " <<
          icon_mapping.page_url.spec();
      continue;
    }
    history_db_->SetFaviconID(url_id, icon_mapping.icon_id);
  }
  return true;
}

bool AndroidProviderBackend::UpdateSearchTermTable() {
  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE,
                                                   kSearchTermUpdateClause));
  while (statement.Step()) {
    string16 term = statement.ColumnString16(0);
    Time last_visit_time = Time::FromInternalValue(statement.ColumnInt64(1));
    SearchTermRow search_term_row;
    if (history_db_->GetSearchTerm(term, &search_term_row) &&
        search_term_row.last_visit_time != last_visit_time) {
      search_term_row.last_visit_time = last_visit_time;
      if (!history_db_->UpdateSearchTerm(search_term_row.id, search_term_row))
        return false;
    } else {
      if (!history_db_->AddSearchTerm(term, last_visit_time))
        return false;
    }
  }
  if (!history_db_->DeleteUnusedSearchTerms())
    return false;

  return true;
}

int AndroidProviderBackend::AppendBookmarkResultColumn(
    const std::vector<BookmarkRow::BookmarkColumnID>& projections,
    std::string* result_column) {
  int replaced_index = -1;
  // Attach the projections
  bool first = true;
  int index = 0;
  for (std::vector<BookmarkRow::BookmarkColumnID>::const_iterator i =
           projections.begin(); i != projections.end(); ++i) {
    if (first)
      first = false;
    else
      result_column->append(", ");

    if (*i == BookmarkRow::FAVICON)
      replaced_index = index;

    result_column->append(BookmarkRow::GetAndroidName(*i));
    index++;
  }
  return replaced_index;
}

bool AndroidProviderBackend::GetSelectedURLs(
    const std::string& selection,
    const std::vector<string16>& selection_args,
    const char* virtual_table,
    TableIDRows* rows) {
  std::string sql("SELECT url_id, urls_url FROM (");
  sql.append(virtual_table);
  sql.append(" )");

  if (!selection.empty()) {
    sql.append(" WHERE ");
    sql.append(selection);
  }

  sql::Statement statement(db_->GetUniqueStatement(sql.c_str()));
  int count = 0;
  BindStatement(selection_args, &statement, &count);
  if (!statement.is_valid()) {
    LOG(ERROR) << db_->GetErrorMessage();
    return false;
  }
  while (statement.Step()) {
    TableIDRow row;
    row.url_id = statement.ColumnInt64(0);
    row.url = GURL(statement.ColumnString(1));
    rows->push_back(row);
  }
  return true;
}

bool AndroidProviderBackend::GetSelectedSearchTerms(
    const std::string& selection,
    const std::vector<string16>& selection_args,
    SearchTerms* rows) {
  std::string sql("SELECT search "
                  "FROM android_cache_db.search_terms ");
  if (!selection.empty()) {
    sql.append(" WHERE ");
    sql.append(selection);
  }
  sql::Statement statement(db_->GetUniqueStatement(sql.c_str()));
  int count = 0;
  BindStatement(selection_args, &statement, &count);
  if (!statement.is_valid()) {
    LOG(ERROR) << db_->GetErrorMessage();
    return false;
  }
  while (statement.Step()) {
    rows->push_back(statement.ColumnString16(0));
  }
  return true;
}

void AndroidProviderBackend::AppendSearchResultColumn(
    const std::vector<SearchRow::SearchColumnID>& projections,
    std::string* result_column) {
  bool first = true;
  int index = 0;
  for (std::vector<SearchRow::SearchColumnID>::const_iterator i =
           projections.begin(); i != projections.end(); ++i) {
    if (first)
      first = false;
    else
      result_column->append(", ");

    result_column->append(SearchRow::GetAndroidName(*i));
    index++;
  }
}

bool AndroidProviderBackend::SimulateUpdateURL(
    const BookmarkRow& row,
    const TableIDRows& ids,
    HistoryNotifications* notifications) {
  DCHECK(ids.size() == 1);
  // URL can not be updated, we simulate the update by deleting the old URL
  // and inserting the new one; We do update the android_urls table as the id
  // need to keep same.

  // Find all columns value of the current URL.
  std::vector<BookmarkRow::BookmarkColumnID> projections;
  projections.push_back(BookmarkRow::LAST_VISIT_TIME);
  projections.push_back(BookmarkRow::CREATED);
  projections.push_back(BookmarkRow::VISIT_COUNT);
  projections.push_back(BookmarkRow::TITLE);
  projections.push_back(BookmarkRow::FAVICON);
  projections.push_back(BookmarkRow::BOOKMARK);

  std::ostringstream oss;
  oss << "url_id = " << ids[0].url_id;

  scoped_ptr<AndroidStatement> statement;
  statement.reset(QueryBookmarksInternal(projections, oss.str(),
      std::vector<string16>(), std::string()));
  if (!statement.get() || !statement->statement()->Step())
    return false;

  BookmarkRow new_row;
  new_row.set_last_visit_time(MillisecondsToTime(
      statement->statement()->ColumnInt64(0)));
  new_row.set_created(MillisecondsToTime(
      statement->statement()->ColumnInt64(1)));
  new_row.set_visit_count(statement->statement()->ColumnInt(2));
  new_row.set_title(statement->statement()->ColumnString16(3));

  scoped_ptr<URLsDeletedDetails> deleted_details(new URLsDeletedDetails);
  scoped_ptr<FaviconChangeDetails> favicon_details(new FaviconChangeDetails);
  scoped_ptr<URLsModifiedDetails> modified(new URLsModifiedDetails);
  URLRow old_url_row;
  if (!history_db_->GetURLRow(ids[0].url_id, &old_url_row))
    return false;
  deleted_details->rows.push_back(old_url_row);
  deleted_details->urls.insert(old_url_row.url());

  FaviconID favicon_id = statement->statement()->ColumnInt64(4);
  if (favicon_id) {
    std::vector<unsigned char> favicon;
    if (!thumbnail_db_->GetFavicon(favicon_id, NULL, &favicon, NULL))
      return false;
    if (!favicon.empty())
      new_row.set_favicon(favicon);
    favicon_details->urls.insert(old_url_row.url());
    favicon_details->urls.insert(row.url());
  }
  new_row.set_is_bookmark(statement->statement()->ColumnBool(5));

  // The SQLHandler vector is not used here because the row in android_url
  // shouldn't be deleted, we need keep the AndroidUIID unchanged, so it
  // appears update to the client.
  if (!urls_handler_->Delete(ids))
    return false;

  if (!visit_handler_->Delete(ids))
    return false;

  if (!favicon_handler_->Delete(ids))
    return false;

  if (!bookmark_model_handler_->Delete(ids))
    return false;

  new_row.set_url(row.url());
  new_row.set_raw_url(row.raw_url());
  if (row.is_value_set_explicitly(BookmarkRow::LAST_VISIT_TIME))
    new_row.set_last_visit_time(row.last_visit_time());
  if (row.is_value_set_explicitly(BookmarkRow::CREATED))
    new_row.set_created(row.created());
  if (row.is_value_set_explicitly(BookmarkRow::VISIT_COUNT))
    new_row.set_visit_count(row.visit_count());
  if (row.is_value_set_explicitly(BookmarkRow::TITLE))
    new_row.set_title(row.title());
  if (row.is_value_set_explicitly(BookmarkRow::FAVICON)) {
    new_row.set_favicon(row.favicon());
    favicon_details->urls.insert(new_row.url());
  }
  if (row.is_value_set_explicitly(BookmarkRow::BOOKMARK))
    new_row.set_is_bookmark(row.is_bookmark());

  if (!urls_handler_->Insert(&new_row))
    return false;

  if (!visit_handler_->Insert(&new_row))
    return false;

  // Update the current row instead of inserting a new row in android urls
  // table. We need keep the AndroidUIID unchanged, so it appears update
  // to the client.
  if (!android_urls_handler_->Update(new_row, ids))
    return false;

  if (!favicon_handler_->Insert(&new_row))
    return false;

  if (!bookmark_model_handler_->Insert(&new_row))
    return false;

  URLRow new_url_row;
  if (!history_db_->GetURLRow(new_row.url_id(), &new_url_row))
    return false;

  modified->changed_urls.push_back(new_url_row);

  notifications->push_back(HistoryNotification(
      chrome::NOTIFICATION_HISTORY_URLS_DELETED,
      deleted_details.release()));
  if (favicon_details.get() && !favicon_details->urls.empty())
    notifications->push_back(HistoryNotification(
        chrome::NOTIFICATION_FAVICON_CHANGED, favicon_details.release()));
  notifications->push_back(HistoryNotification(
      chrome::NOTIFICATION_HISTORY_TYPED_URLS_MODIFIED, modified.release()));

  return true;
}

AndroidStatement* AndroidProviderBackend::QueryBookmarksInternal(
    const std::vector<BookmarkRow::BookmarkColumnID>& projections,
    const std::string& selection,
    const std::vector<string16>& selection_args,
    const std::string& sort_order) {
  std::string sql;
  sql.append("SELECT ");
  int replaced_index = AppendBookmarkResultColumn(projections, &sql);
  sql.append(" FROM (");
  sql.append(kVirtualBookmarkTable);
  sql.append(")");

  if (!selection.empty()) {
    sql.append(" WHERE ");
    sql.append(selection);
  }

  if (!sort_order.empty()) {
    sql.append(" ORDER BY ");
    sql.append(sort_order);
  }

  scoped_ptr<sql::Statement> statement(new sql::Statement(
      db_->GetUniqueStatement(sql.c_str())));
  int count = 0;
  BindStatement(selection_args, statement.get(), &count);
  if (!statement->is_valid()) {
    LOG(ERROR) << db_->GetErrorMessage();
    return NULL;
  }
  sql::Statement* result = statement.release();
  return new AndroidStatement(result, replaced_index);
}

void AndroidProviderBackend::BroadcastNotifications(
    const HistoryNotifications& notifications) {
  for (HistoryNotifications::const_iterator i = notifications.begin();
       i != notifications.end(); ++i) {
    delegate_->BroadcastNotifications(i->type, i->detail);
  }
}

bool AndroidProviderBackend::AddSearchTerm(const SearchRow& values) {
  DCHECK(values.is_value_set_explicitly(SearchRow::SEARCH_TERM));
  DCHECK(values.is_value_set_explicitly(SearchRow::TEMPLATE_URL));
  DCHECK(values.is_value_set_explicitly(SearchRow::URL));

  URLRow url_row;
  BookmarkRow bookmark_row;
  // Android CTS test BrowserTest.testAccessSearches allows insert the same
  // seach term multiple times, and just search time need updated.
  if (history_db_->GetRowForURL(values.url(), &url_row)) {
    // Already exist, Add a visit.
    if (values.is_value_set_explicitly(SearchRow::SEARCH_TIME))
      bookmark_row.set_last_visit_time(values.search_time());
    else
      bookmark_row.set_visit_count(url_row.visit_count() + 1);
    TableIDRows table_id_rows;
    TableIDRow table_id_row;
    table_id_row.url = values.url();
    table_id_row.url_id = url_row.id();
    table_id_rows.push_back(table_id_row);
    if (!urls_handler_->Update(bookmark_row, table_id_rows))
      return false;
    if (!visit_handler_->Update(bookmark_row, table_id_rows))
      return false;

    if (!history_db_->GetKeywordSearchTermRow(url_row.id(), NULL))
      if (!history_db_->SetKeywordSearchTermsForURL(url_row.id(),
               values.template_url_id(), values.search_term()))
        return false;
  } else {
    bookmark_row.set_raw_url(values.url().spec());
    bookmark_row.set_url(values.url());
    if (values.is_value_set_explicitly(SearchRow::SEARCH_TIME))
      bookmark_row.set_last_visit_time(values.search_time());

    if (!urls_handler_->Insert(&bookmark_row))
      return false;

    if (!visit_handler_->Insert(&bookmark_row))
      return false;

    if (!android_urls_handler_->Insert(&bookmark_row))
      return false;

    if (!history_db_->SetKeywordSearchTermsForURL(bookmark_row.url_id(),
                          values.template_url_id(), values.search_term()))
      return false;
  }
  return true;
}

}  // namespace history
