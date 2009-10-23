// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cookies_table_model.h"

#include "app/l10n_util.h"
#include "app/table_model_observer.h"
#include "app/resource_bundle.h"
#include "base/string_util.h"
#include "chrome/browser/net/chrome_url_request_context.h"
#include "chrome/browser/profile.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "net/url_request/url_request_context.h"
#include "third_party/skia/include/core/SkBitmap.h"

///////////////////////////////////////////////////////////////////////////////
// CookiesTableModel, public:

CookiesTableModel::CookiesTableModel(Profile* profile)
    : profile_(profile) {
  LoadCookies();
}

std::string CookiesTableModel::GetDomainAt(int index) {
  DCHECK(index >= 0 && index < RowCount());
  return shown_cookies_.at(index)->first;
}

net::CookieMonster::CanonicalCookie& CookiesTableModel::GetCookieAt(
    int index) {
  DCHECK(index >= 0 && index < RowCount());
  return shown_cookies_.at(index)->second;
}

void CookiesTableModel::RemoveCookies(int start_index, int remove_count) {
  if (remove_count <= 0) {
    NOTREACHED();
    return;
  }
  // Since we are running on the UI thread don't call GetURLRequestContext().
  net::CookieMonster* monster =
      profile_->GetRequestContext()->GetCookieStore()->GetCookieMonster();

  // We need to update the searched results list, the full cookie list,
  // and the view.  We walk through the search results list (which is what
  // is displayed) and map these back to the full cookie list.  They should
  // be in the same sort order, and always exist, so we can just walk once.
  // We can't delete any entries from all_cookies_ without invaliding all of
  // our pointers after it (which are in shown_cookies), so we go backwards.
  CookiePtrList::iterator first = shown_cookies_.begin() + start_index;
  CookiePtrList::iterator last = first + remove_count;
  CookieList::iterator all_it = all_cookies_.end();
  while (last != first) {
    --last;
    --all_it;
    // Seek to the corresponding entry in all_cookies_
    while (&*all_it != *last) --all_it;
    // Delete the cookie from the monster
    monster->DeleteCookie(all_it->first, all_it->second, true);
    all_it = all_cookies_.erase(all_it);
  }

  // By deleting entries from all_cookies, we just possibly moved stuff around
  // and have thus invalidated all of our pointers, so rebuild shown_cookies.
  // We could do this all better if there was a way to mark elements of
  // all_cookies as dead instead of deleting, but this should be fine for now.
  DoFilter();
  if (observer_)
    observer_->OnItemsRemoved(start_index, remove_count);
}

void CookiesTableModel::RemoveAllShownCookies() {
  RemoveCookies(0, RowCount());
}

///////////////////////////////////////////////////////////////////////////////
// CookiesTableModel, TableModel implementation:

int CookiesTableModel::RowCount() {
  return static_cast<int>(shown_cookies_.size());
}

std::wstring CookiesTableModel::GetText(int row, int column_id) {
  DCHECK(row >= 0 && row < RowCount());
  switch (column_id) {
    case IDS_COOKIES_DOMAIN_COLUMN_HEADER:
      {
        // Domain cookies start with a trailing dot, but we will show this
        // in the cookie details, show it without the dot in the list.
        std::string& domain = shown_cookies_.at(row)->first;
        std::wstring wide_domain;
        if (!domain.empty() && domain[0] == '.')
          wide_domain = UTF8ToWide(domain.substr(1));
        else
          wide_domain = UTF8ToWide(domain);
        // Force domain to be LTR
        if (l10n_util::GetTextDirection() == l10n_util::RIGHT_TO_LEFT)
          l10n_util::WrapStringWithLTRFormatting(&wide_domain);
        return wide_domain;
      }
      break;
    case IDS_COOKIES_NAME_COLUMN_HEADER: {
      std::wstring name = UTF8ToWide(shown_cookies_.at(row)->second.Name());
      l10n_util::AdjustStringForLocaleDirection(name, &name);
      return name;
      break;
    }
  }
  NOTREACHED();
  return L"";
}

SkBitmap CookiesTableModel::GetIcon(int row) {
  static SkBitmap* icon = ResourceBundle::GetSharedInstance().GetBitmapNamed(
      IDR_COOKIE_ICON);
  return *icon;
}

void CookiesTableModel::SetObserver(TableModelObserver* observer) {
  observer_ = observer;
}

int CookiesTableModel::CompareValues(int row1, int row2, int column_id) {
  if (column_id == IDS_COOKIES_DOMAIN_COLUMN_HEADER) {
    // Sort ignore the '.' prefix for domain cookies.
    net::CookieMonster::CookieListPair* cp1 = shown_cookies_[row1];
    net::CookieMonster::CookieListPair* cp2 = shown_cookies_[row2];
    bool is1domain = !cp1->first.empty() && cp1->first[0] == '.';
    bool is2domain = !cp2->first.empty() && cp2->first[0] == '.';

    // They are both either domain or host cookies, sort them normally.
    if (is1domain == is2domain)
      return cp1->first.compare(cp2->first);

    // One (but only one) is a domain cookie, skip the beginning '.'.
    return is1domain ?
        cp1->first.compare(1, cp1->first.length() - 1, cp2->first) :
        -cp2->first.compare(1, cp2->first.length() - 1, cp1->first);
  }
  return TableModel::CompareValues(row1, row2, column_id);
}

///////////////////////////////////////////////////////////////////////////////
// CookiesTableModel, private:

// Returns true if |cookie| matches the specified filter, where "match" is
// defined as the cookie's domain, name and value contains filter text
// somewhere.
static bool ContainsFilterText(
    const std::string& domain,
    const net::CookieMonster::CanonicalCookie& cookie,
    const std::string& filter) {
  return domain.find(filter) != std::string::npos ||
      cookie.Name().find(filter) != std::string::npos ||
      cookie.Value().find(filter) != std::string::npos;
}

void CookiesTableModel::LoadCookies() {
  // mmargh mmargh mmargh!

  // Since we are running on the UI thread don't call GetURLRequestContext().
  net::CookieMonster* cookie_monster =
      profile_->GetRequestContext()->GetCookieStore()->GetCookieMonster();

  all_cookies_ = cookie_monster->GetAllCookies();
  DoFilter();
}

void CookiesTableModel::DoFilter() {
  std::string utf8_filter = WideToUTF8(filter_);
  bool has_filter = !utf8_filter.empty();

  shown_cookies_.clear();

  CookieList::iterator iter = all_cookies_.begin();
  for (; iter != all_cookies_.end(); ++iter) {
    if (!has_filter ||
        ContainsFilterText(iter->first, iter->second, utf8_filter)) {
      shown_cookies_.push_back(&*iter);
    }
  }
}

void CookiesTableModel::UpdateSearchResults(const std::wstring& filter) {
  filter_ = filter;
  DoFilter();
  observer_->OnModelChanged();
}
