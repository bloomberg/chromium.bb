// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/starred_url_database.h"

#include "app/sql/statement.h"
#include "base/file_util.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/scoped_vector.h"
#include "base/stl_util-inl.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/bookmarks/bookmark_codec.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/history/query_parser.h"

// The following table is used to store star (aka bookmark) information. This
// class derives from URLDatabase, which has its own schema.
//
// starred
//   id                 Unique identifier (primary key) for the entry.
//   type               Type of entry, if 0 this corresponds to a URL, 1 for
//                      a system folder, 2 for a user created folder, 3 for
//                      other.
//   url_id             ID of the url, only valid if type == 0
//   group_id           ID of the folder, only valid if type != 0. This id comes
//                      from the UI and is NOT the same as id.
//   title              User assigned title.
//   date_added         Creation date.
//   visual_order       Visual order within parent.
//   parent_id          Folder ID of the parent this entry is contained in, if 0
//                      entry is not in a folder.
//   date_modified      Time the folder was last modified. See comments in
//                      StarredEntry::date_folder_modified
// NOTE: group_id and parent_id come from the UI, id is assigned by the
// db.

namespace history {

namespace {

// Fields used by FillInStarredEntry.
#define STAR_FIELDS \
    " starred.id, starred.type, starred.title, starred.date_added, " \
    "starred.visual_order, starred.parent_id, urls.url, urls.id, " \
    "starred.group_id, starred.date_modified "
const char kHistoryStarFields[] = STAR_FIELDS;

void FillInStarredEntry(const sql::Statement& s, StarredEntry* entry) {
  DCHECK(entry);
  entry->id = s.ColumnInt64(0);
  switch (s.ColumnInt(1)) {
    case 0:
      entry->type = history::StarredEntry::URL;
      entry->url = GURL(s.ColumnString(6));
      break;
    case 1:
      entry->type = history::StarredEntry::BOOKMARK_BAR;
      break;
    case 2:
      entry->type = history::StarredEntry::USER_FOLDER;
      break;
    case 3:
      entry->type = history::StarredEntry::OTHER;
      break;
    default:
      NOTREACHED();
      break;
  }
  entry->title = s.ColumnString16(2);
  entry->date_added = base::Time::FromInternalValue(s.ColumnInt64(3));
  entry->visual_order = s.ColumnInt(4);
  entry->parent_folder_id = s.ColumnInt64(5);
  entry->url_id = s.ColumnInt64(7);
  entry->folder_id = s.ColumnInt64(8);
  entry->date_folder_modified = base::Time::FromInternalValue(s.ColumnInt64(9));
}

}  // namespace

StarredURLDatabase::StarredURLDatabase() {
}

StarredURLDatabase::~StarredURLDatabase() {
}

bool StarredURLDatabase::MigrateBookmarksToFile(const FilePath& path) {
  if (!GetDB().DoesTableExist("starred"))
    return true;

  if (EnsureStarredIntegrity() && !MigrateBookmarksToFileImpl(path)) {
    NOTREACHED() << " Bookmarks migration failed";
    return false;
  }

  if (!GetDB().Execute("DROP TABLE starred")) {
    NOTREACHED() << "Unable to drop starred table";
    return false;
  }
  return true;
}

bool StarredURLDatabase::GetAllStarredEntries(
    std::vector<StarredEntry>* entries) {
  DCHECK(entries);
  std::string sql = "SELECT ";
  sql.append(kHistoryStarFields);
  sql.append("FROM starred LEFT JOIN urls ON starred.url_id = urls.id ");
  sql += "ORDER BY parent_id, visual_order";

  sql::Statement s(GetDB().GetUniqueStatement(sql.c_str()));
  if (!s) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  history::StarredEntry entry;
  while (s.Step()) {
    FillInStarredEntry(s, &entry);
    // Reset the url for non-url types. This is needed as we're reusing the
    // same entry for the loop.
    if (entry.type != history::StarredEntry::URL)
      entry.url = GURL();
    entries->push_back(entry);
  }
  return true;
}

bool StarredURLDatabase::EnsureStarredIntegrity() {
  std::set<StarredNode*> roots;
  std::set<StarID> folders_with_duplicate_ids;
  std::set<StarredNode*> unparented_urls;
  std::set<StarID> empty_url_ids;

  if (!BuildStarNodes(&roots, &folders_with_duplicate_ids, &unparented_urls,
                      &empty_url_ids)) {
    return false;
  }

  bool valid = EnsureStarredIntegrityImpl(&roots, folders_with_duplicate_ids,
                                          &unparented_urls, empty_url_ids);

  STLDeleteElements(&roots);
  STLDeleteElements(&unparented_urls);
  return valid;
}

bool StarredURLDatabase::UpdateStarredEntryRow(StarID star_id,
                                               const string16& title,
                                               UIStarID parent_folder_id,
                                               int visual_order,
                                               base::Time date_modified) {
  DCHECK(star_id && visual_order >= 0);
  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "UPDATE starred SET title=?, parent_id=?, visual_order=?, "
      "date_modified=? WHERE id=?"));
  if (!statement)
    return 0;

  statement.BindString16(0, title);
  statement.BindInt64(1, parent_folder_id);
  statement.BindInt(2, visual_order);
  statement.BindInt64(3, date_modified.ToInternalValue());
  statement.BindInt64(4, star_id);
  return statement.Run();
}

bool StarredURLDatabase::AdjustStarredVisualOrder(UIStarID parent_folder_id,
                                                  int start_visual_order,
                                                  int delta) {
  DCHECK(parent_folder_id && start_visual_order >= 0);
  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "UPDATE starred SET visual_order=visual_order+? "
      "WHERE parent_id=? AND visual_order >= ?"));
  if (!statement)
    return false;

  statement.BindInt(0, delta);
  statement.BindInt64(1, parent_folder_id);
  statement.BindInt(2, start_visual_order);
  return statement.Run();
}

StarID StarredURLDatabase::CreateStarredEntryRow(URLID url_id,
                                                 UIStarID folder_id,
                                                 UIStarID parent_folder_id,
                                                 const string16& title,
                                                 const base::Time& date_added,
                                                 int visual_order,
                                                 StarredEntry::Type type) {
  DCHECK(visual_order >= 0 &&
         (type != history::StarredEntry::URL || url_id));
  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "INSERT INTO starred "
      "(type, url_id, group_id, title, date_added, visual_order, parent_id, "
      "date_modified) VALUES (?,?,?,?,?,?,?,?)"));
  if (!statement)
    return 0;

  switch (type) {
    case history::StarredEntry::URL:
      statement.BindInt(0, 0);
      break;
    case history::StarredEntry::BOOKMARK_BAR:
      statement.BindInt(0, 1);
      break;
    case history::StarredEntry::USER_FOLDER:
      statement.BindInt(0, 2);
      break;
    case history::StarredEntry::OTHER:
      statement.BindInt(0, 3);
      break;
    default:
      NOTREACHED();
  }
  statement.BindInt64(1, url_id);
  statement.BindInt64(2, folder_id);
  statement.BindString16(3, title);
  statement.BindInt64(4, date_added.ToInternalValue());
  statement.BindInt(5, visual_order);
  statement.BindInt64(6, parent_folder_id);
  statement.BindInt64(7, base::Time().ToInternalValue());
  if (statement.Run())
    return GetDB().GetLastInsertRowId();
  return 0;
}

bool StarredURLDatabase::DeleteStarredEntryRow(StarID star_id) {
  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "DELETE FROM starred WHERE id=?"));
  if (!statement)
    return false;

  statement.BindInt64(0, star_id);
  return statement.Run();
}

bool StarredURLDatabase::GetStarredEntry(StarID star_id, StarredEntry* entry) {
  DCHECK(entry && star_id);
  sql::Statement statement(GetDB().GetCachedStatement(SQL_FROM_HERE,
      "SELECT" STAR_FIELDS "FROM starred LEFT JOIN urls ON "
      "starred.url_id = urls.id WHERE starred.id=?"));
  if (!statement)
    return false;

  statement.BindInt64(0, star_id);

  if (statement.Step()) {
    FillInStarredEntry(statement, entry);
    return true;
  }
  return false;
}

StarID StarredURLDatabase::CreateStarredEntry(StarredEntry* entry) {
  entry->id = 0;  // Ensure 0 for failure case.

  // Adjust the visual order when we are inserting it somewhere.
  if (entry->parent_folder_id)
    AdjustStarredVisualOrder(entry->parent_folder_id, entry->visual_order, 1);

  // Insert the new entry.
  switch (entry->type) {
    case StarredEntry::USER_FOLDER:
      entry->id = CreateStarredEntryRow(0, entry->folder_id,
          entry->parent_folder_id, entry->title, entry->date_added,
          entry->visual_order, entry->type);
      break;

    case StarredEntry::URL: {
      // Get the row for this URL.
      URLRow url_row;
      if (!GetRowForURL(entry->url, &url_row)) {
        // Create a new URL row for this entry.
        url_row = URLRow(entry->url);
        url_row.set_title(entry->title);
        url_row.set_hidden(false);
        entry->url_id = this->AddURL(url_row);
      } else {
        entry->url_id = url_row.id();  // The caller doesn't have to set this.
      }

      // Create the star entry referring to the URL row.
      entry->id = CreateStarredEntryRow(entry->url_id, entry->folder_id,
          entry->parent_folder_id, entry->title, entry->date_added,
          entry->visual_order, entry->type);

      // Update the URL row to refer to this new starred entry.
      UpdateURLRow(entry->url_id, url_row);
      break;
    }

    default:
      NOTREACHED();
      break;
  }
  return entry->id;
}

UIStarID StarredURLDatabase::GetMaxFolderID() {
  sql::Statement max_folder_id_statement(GetDB().GetUniqueStatement(
      "SELECT MAX(group_id) FROM starred"));
  if (!max_folder_id_statement) {
    NOTREACHED() << GetDB().GetErrorMessage();
    return 0;
  }
  if (!max_folder_id_statement.Step()) {
    NOTREACHED() << GetDB().GetErrorMessage();
    return 0;
  }
  return max_folder_id_statement.ColumnInt64(0);
}

bool StarredURLDatabase::BuildStarNodes(
    std::set<StarredURLDatabase::StarredNode*>* roots,
    std::set<StarID>* folders_with_duplicate_ids,
    std::set<StarredNode*>* unparented_urls,
    std::set<StarID>* empty_url_ids) {
  std::vector<StarredEntry> star_entries;
  if (!GetAllStarredEntries(&star_entries)) {
    NOTREACHED() << "Unable to get bookmarks from database";
    return false;
  }

  // Create the folder/bookmark-bar/other nodes.
  std::map<UIStarID, StarredNode*> folder_id_to_node_map;
  for (size_t i = 0; i < star_entries.size(); ++i) {
    if (star_entries[i].type != StarredEntry::URL) {
      if (folder_id_to_node_map.find(star_entries[i].folder_id) !=
          folder_id_to_node_map.end()) {
        // There's already a folder with this ID.
        folders_with_duplicate_ids->insert(star_entries[i].id);
      } else {
        // Create the node and update the mapping.
        StarredNode* node = new StarredNode(star_entries[i]);
        folder_id_to_node_map[star_entries[i].folder_id] = node;
      }
    }
  }

  // Iterate again, creating nodes for URL bookmarks and parenting all
  // bookmarks/folders. In addition populate the empty_url_ids with all entries
  // of type URL that have an empty URL.
  std::map<StarID, StarredNode*> id_to_node_map;
  for (size_t i = 0; i < star_entries.size(); ++i) {
    if (star_entries[i].type == StarredEntry::URL) {
      if (star_entries[i].url.is_empty()) {
        empty_url_ids->insert(star_entries[i].id);
      } else if (!star_entries[i].parent_folder_id ||
          folder_id_to_node_map.find(star_entries[i].parent_folder_id) ==
          folder_id_to_node_map.end()) {
        // This entry has no parent, or we couldn't find the parent.
        StarredNode* node = new StarredNode(star_entries[i]);
        unparented_urls->insert(node);
      } else {
        // Add the node to its parent.
        StarredNode* parent =
            folder_id_to_node_map[star_entries[i].parent_folder_id];
        StarredNode* node = new StarredNode(star_entries[i]);
        parent->Add(node, parent->child_count());
      }
    } else if (folders_with_duplicate_ids->find(star_entries[i].id) ==
               folders_with_duplicate_ids->end()) {
      // The entry is a folder (or bookmark bar/other node) that isn't
      // marked as a duplicate.
      if (!star_entries[i].parent_folder_id ||
          folder_id_to_node_map.find(star_entries[i].parent_folder_id) ==
          folder_id_to_node_map.end()) {
        // Entry has no parent, or the parent wasn't found.
        roots->insert(folder_id_to_node_map[star_entries[i].folder_id]);
      } else {
        // Parent the folder node.
        StarredNode* parent =
            folder_id_to_node_map[star_entries[i].parent_folder_id];
        StarredNode* node = folder_id_to_node_map[star_entries[i].folder_id];
        if (!node->HasAncestor(parent) && !parent->HasAncestor(node)) {
          parent->Add(node, parent->child_count());
        } else {
          // The node has a cycle. Add it to the list of roots so the cycle is
          // broken.
          roots->insert(node);
        }
      }
    }
  }
  return true;
}

StarredURLDatabase::StarredNode* StarredURLDatabase::GetNodeByType(
    const std::set<StarredURLDatabase::StarredNode*>& nodes,
    StarredEntry::Type type) {
  for (std::set<StarredNode*>::const_iterator i = nodes.begin();
       i != nodes.end(); ++i) {
    if ((*i)->value.type == type)
      return *i;
  }
  return NULL;
}

bool StarredURLDatabase::EnsureVisualOrder(
    StarredURLDatabase::StarredNode* node) {
  for (int i = 0; i < node->child_count(); ++i) {
    if (node->GetChild(i)->value.visual_order != i) {
      StarredEntry& entry = node->GetChild(i)->value;
      entry.visual_order = i;
      LOG(WARNING) << "Bookmark visual order is wrong";
      if (!UpdateStarredEntryRow(entry.id, entry.title, entry.parent_folder_id,
                                 i, entry.date_folder_modified)) {
        NOTREACHED() << "Unable to update visual order";
        return false;
      }
    }
    if (!EnsureVisualOrder(node->GetChild(i)))
      return false;
  }
  return true;
}

bool StarredURLDatabase::EnsureStarredIntegrityImpl(
    std::set<StarredURLDatabase::StarredNode*>* roots,
    const std::set<StarID>& folders_with_duplicate_ids,
    std::set<StarredNode*>* unparented_urls,
    const std::set<StarID>& empty_url_ids) {
  // Make sure the bookmark bar entry exists.
  StarredNode* bookmark_node =
      GetNodeByType(*roots, StarredEntry::BOOKMARK_BAR);
  if (!bookmark_node) {
    LOG(WARNING) << "No bookmark bar folder in database";
    // If there is no bookmark bar entry in the db things are really
    // screwed. Return false, which won't trigger migration and we'll just
    // drop the tables.
    return false;
  }

  // Make sure the other node exists.
  StarredNode* other_node = GetNodeByType(*roots, StarredEntry::OTHER);
  if (!other_node) {
    LOG(WARNING) << "No bookmark other folder in database";
    StarredEntry entry;
    entry.folder_id = GetMaxFolderID() + 1;
    if (entry.folder_id == 1) {
      NOTREACHED() << "Unable to get new id for other bookmarks folder";
      return false;
    }
    entry.id = CreateStarredEntryRow(
        0, entry.folder_id, 0, UTF8ToUTF16("other"), base::Time::Now(), 0,
        history::StarredEntry::OTHER);
    if (!entry.id) {
      NOTREACHED() << "Unable to create other bookmarks folder";
      return false;
    }
    entry.type = StarredEntry::OTHER;
    StarredNode* other_node = new StarredNode(entry);
    roots->insert(other_node);
  }

  // We could potentially make sure only one folder with type
  // BOOKMARK_BAR/OTHER, but history backend enforces this.

  // Nuke any entries with no url.
  for (std::set<StarID>::const_iterator i = empty_url_ids.begin();
       i != empty_url_ids.end(); ++i) {
    LOG(WARNING) << "Bookmark exists with no URL";
    if (!DeleteStarredEntryRow(*i)) {
      NOTREACHED() << "Unable to delete bookmark";
      return false;
    }
  }

  // Make sure the visual order of the nodes is correct.
  for (std::set<StarredNode*>::const_iterator i = roots->begin();
       i != roots->end(); ++i) {
    if (!EnsureVisualOrder(*i))
      return false;
  }

  // Move any unparented bookmarks to the bookmark bar.
  {
    std::set<StarredNode*>::iterator i = unparented_urls->begin();
    while (i != unparented_urls->end()) {
      LOG(WARNING) << "Bookmark not in a bookmark folder found";
      if (!Move(*i, bookmark_node))
        return false;
      unparented_urls->erase(i++);
    }
  }

  // Nuke any folders with duplicate ids. A duplicate id means there are two
  // folders in the starred table with the same folder_id. We only keep the
  // first folder, all other folders are removed.
  for (std::set<StarID>::const_iterator i = folders_with_duplicate_ids.begin();
       i != folders_with_duplicate_ids.end(); ++i) {
    LOG(WARNING) << "Duplicate folder id in bookmark database";
    if (!DeleteStarredEntryRow(*i)) {
      NOTREACHED() << "Unable to delete folder";
      return false;
    }
  }

  // Move unparented user folders back to the bookmark bar.
  {
    std::set<StarredNode*>::iterator i = roots->begin();
    while (i != roots->end()) {
      if ((*i)->value.type == StarredEntry::USER_FOLDER) {
        LOG(WARNING) << "Bookmark folder not on bookmark bar found";
        if (!Move(*i, bookmark_node))
          return false;
        roots->erase(i++);
      } else {
        ++i;
      }
    }
  }

  return true;
}

bool StarredURLDatabase::Move(StarredNode* source, StarredNode* new_parent) {
  history::StarredEntry& entry = source->value;
  entry.visual_order = new_parent->child_count();
  entry.parent_folder_id = new_parent->value.folder_id;
  if (!UpdateStarredEntryRow(entry.id, entry.title,
                             entry.parent_folder_id, entry.visual_order,
                             entry.date_folder_modified)) {
    NOTREACHED() << "Unable to move folder";
    return false;
  }
  new_parent->Add(source, new_parent->child_count());
  return true;
}

bool StarredURLDatabase::MigrateBookmarksToFileImpl(const FilePath& path) {
  std::vector<history::StarredEntry> entries;
  if (!GetAllStarredEntries(&entries))
    return false;

  // Create the bookmark bar and other folder nodes.
  history::StarredEntry entry;
  entry.type = history::StarredEntry::BOOKMARK_BAR;
  BookmarkNode bookmark_bar_node(0, GURL());
  bookmark_bar_node.Reset(entry);
  entry.type = history::StarredEntry::OTHER;
  BookmarkNode other_node(0, GURL());
  other_node.Reset(entry);

  std::map<history::UIStarID, history::StarID> folder_id_to_id_map;
  typedef std::map<history::StarID, BookmarkNode*> IDToNodeMap;
  IDToNodeMap id_to_node_map;

  history::UIStarID other_folder_folder_id = 0;
  history::StarID other_folder_id = 0;

  // Iterate through the entries building a mapping between folder_id and id.
  for (std::vector<history::StarredEntry>::const_iterator i = entries.begin();
       i != entries.end(); ++i) {
    if (i->type != history::StarredEntry::URL) {
      folder_id_to_id_map[i->folder_id] = i->id;
      if (i->type == history::StarredEntry::OTHER) {
        other_folder_id = i->id;
        other_folder_folder_id = i->folder_id;
      }
    }
  }

  // Register the bookmark bar and other folder nodes in the maps.
  id_to_node_map[HistoryService::kBookmarkBarID] = &bookmark_bar_node;
  folder_id_to_id_map[HistoryService::kBookmarkBarID] =
      HistoryService::kBookmarkBarID;
  if (other_folder_folder_id) {
    id_to_node_map[other_folder_id] = &other_node;
    folder_id_to_id_map[other_folder_folder_id] = other_folder_id;
  }

  // Iterate through the entries again creating the nodes.
  for (std::vector<history::StarredEntry>::iterator i = entries.begin();
       i != entries.end(); ++i) {
    if (!i->parent_folder_id) {
      DCHECK(i->type == history::StarredEntry::BOOKMARK_BAR ||
             i->type == history::StarredEntry::OTHER);
      // Only entries with no parent should be the bookmark bar and other
      // bookmarks folders.
      continue;
    }

    BookmarkNode* node = id_to_node_map[i->id];
    if (!node) {
      // Creating a node results in creating the parent. As such, it is
      // possible for the node representing a folder to have been created before
      // encountering the details.

      // The created nodes are owned by the root node.
      node = new BookmarkNode(0, i->url);
      id_to_node_map[i->id] = node;
    }
    node->Reset(*i);

    DCHECK(folder_id_to_id_map.find(i->parent_folder_id) !=
           folder_id_to_id_map.end());
    history::StarID parent_id = folder_id_to_id_map[i->parent_folder_id];
    BookmarkNode* parent = id_to_node_map[parent_id];
    if (!parent) {
      // Haven't encountered the parent yet, create it now.
      parent = new BookmarkNode(0, GURL());
      id_to_node_map[parent_id] = parent;
    }

    // Add the node to its parent. |entries| is ordered by parent then
    // visual order so that we know we maintain visual order by always adding
    // to the end.
    parent->Add(node, parent->child_count());
  }

  // Save to file.
  BookmarkCodec encoder;
  scoped_ptr<Value> encoded_bookmarks(
      encoder.Encode(&bookmark_bar_node, &other_node));
  std::string content;
  base::JSONWriter::Write(encoded_bookmarks.get(), true, &content);

  return (file_util::WriteFile(path, content.c_str(),
                               static_cast<int>(content.length())) != -1);
}

}  // namespace history
