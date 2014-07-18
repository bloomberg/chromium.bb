// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/reading_list_private/reading_list_private_api.h"

#include <string>
#include <vector>

#include "base/memory/linked_ptr.h"
#include "chrome/browser/dom_distiller/dom_distiller_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/reading_list_private.h"
#include "components/dom_distiller/core/article_entry.h"
#include "components/dom_distiller/core/dom_distiller_service.h"
#include "content/public/browser/web_contents.h"

namespace extensions {

namespace AddEntry = api::reading_list_private::AddEntry;
namespace RemoveEntry = api::reading_list_private::RemoveEntry;
namespace GetEntries = api::reading_list_private::GetEntries;

using api::reading_list_private::Entry;
using dom_distiller::ArticleEntry;
using dom_distiller::DomDistillerService;
using dom_distiller::DomDistillerServiceFactory;

bool ReadingListPrivateAddEntryFunction::RunAsync() {
  scoped_ptr<AddEntry::Params> params(AddEntry::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  GURL url_to_add(params->entry.url);
  if (!url_to_add.is_valid()) {
    error_ = "Invalid url specified.";
    SendResponse(false);
    return false;
  }

  DomDistillerService* service =
      DomDistillerServiceFactory::GetForBrowserContext(GetProfile());
  gfx::Size render_view_size;
  content::WebContents* web_contents = GetAssociatedWebContents();
  if (web_contents)
    render_view_size = web_contents->GetContainerBounds().size();
  const std::string& id = service->AddToList(
      url_to_add,
      service->CreateDefaultDistillerPage(render_view_size).Pass(),
      base::Bind(&ReadingListPrivateAddEntryFunction::SendResponse, this));
  Entry new_entry;
  new_entry.id = id;
  results_ = AddEntry::Results::Create(new_entry);
  return true;
}

bool ReadingListPrivateRemoveEntryFunction::RunSync() {
  scoped_ptr<RemoveEntry::Params> params(RemoveEntry::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  DomDistillerService* service =
      DomDistillerServiceFactory::GetForBrowserContext(GetProfile());
  scoped_ptr<ArticleEntry> entry(service->RemoveEntry(params->id));
  if (entry == NULL) {
    results_ = make_scoped_ptr(new base::ListValue());
  } else {
    Entry removed_entry;
    removed_entry.id = entry->entry_id();
    results_ = RemoveEntry::Results::Create(removed_entry);
  }
  return true;
}

bool ReadingListPrivateGetEntriesFunction::RunSync() {
  DomDistillerService* service =
      DomDistillerServiceFactory::GetForBrowserContext(GetProfile());
  const std::vector<ArticleEntry>& entries = service->GetEntries();
  std::vector<linked_ptr<Entry> > result;
  for (std::vector<ArticleEntry>::const_iterator i = entries.begin();
       i != entries.end();
       ++i) {
    linked_ptr<Entry> e(new Entry);
    e->id = i->entry_id();
    result.push_back(e);
  }
  results_ = GetEntries::Results::Create(result);
  return true;
}

}  // namespace extensions
