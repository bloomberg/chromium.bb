// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/leveldb/leveldb_transaction.h"

#include "base/logging.h"
#include "content/browser/indexed_db/leveldb/leveldb_database.h"
#include "content/browser/indexed_db/leveldb/leveldb_write_batch.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"

using base::StringPiece;

namespace content {

LevelDBTransaction::LevelDBTransaction(LevelDBDatabase* db)
    : db_(db), snapshot_(db), comparator_(db->Comparator()), finished_(false) {
  tree_.abstractor().comparator_ = comparator_;
}

LevelDBTransaction::AVLTreeNode::AVLTreeNode() {}
LevelDBTransaction::AVLTreeNode::~AVLTreeNode() {}

void LevelDBTransaction::ClearTree() {
  TreeType::Iterator iterator;
  iterator.StartIterLeast(&tree_);

  std::vector<AVLTreeNode*> nodes;

  while (*iterator) {
    nodes.push_back(*iterator);
    ++iterator;
  }
  tree_.Purge();

  for (size_t i = 0; i < nodes.size(); ++i)
    delete nodes[i];
}

LevelDBTransaction::~LevelDBTransaction() { ClearTree(); }

void LevelDBTransaction::Set(const StringPiece& key,
                             std::string* value,
                             bool deleted) {
  DCHECK(!finished_);
  bool new_node = false;
  AVLTreeNode* node = tree_.Search(key);

  if (!node) {
    node = new AVLTreeNode;
    node->key = key.as_string();
    tree_.Insert(node);
    new_node = true;
  }
  node->value.swap(*value);
  node->deleted = deleted;

  if (new_node)
    NotifyIteratorsOfTreeChange();
}

void LevelDBTransaction::Put(const StringPiece& key, std::string* value) {
  Set(key, value, false);
}

void LevelDBTransaction::Remove(const StringPiece& key) {
  std::string empty;
  Set(key, &empty, true);
}

bool LevelDBTransaction::Get(const StringPiece& key,
                             std::string* value,
                             bool* found) {
  *found = false;
  DCHECK(!finished_);
  AVLTreeNode* node = tree_.Search(key);

  if (node) {
    if (node->deleted)
      return true;

    *value = node->value;
    *found = true;
    return true;
  }

  bool ok = db_->Get(key, value, found, &snapshot_);
  if (!ok) {
    DCHECK(!*found);
    return false;
  }
  return true;
}

bool LevelDBTransaction::Commit() {
  DCHECK(!finished_);

  if (tree_.IsEmpty()) {
    finished_ = true;
    return true;
  }

  scoped_ptr<LevelDBWriteBatch> write_batch = LevelDBWriteBatch::Create();

  TreeType::Iterator iterator;
  iterator.StartIterLeast(&tree_);

  while (*iterator) {
    AVLTreeNode* node = *iterator;
    if (!node->deleted)
      write_batch->Put(node->key, node->value);
    else
      write_batch->Remove(node->key);
    ++iterator;
  }

  if (!db_->Write(*write_batch))
    return false;

  ClearTree();
  finished_ = true;
  return true;
}

void LevelDBTransaction::Rollback() {
  DCHECK(!finished_);
  finished_ = true;
  ClearTree();
}

scoped_ptr<LevelDBIterator> LevelDBTransaction::CreateIterator() {
  return TransactionIterator::Create(this).PassAs<LevelDBIterator>();
}

scoped_ptr<LevelDBTransaction::TreeIterator>
LevelDBTransaction::TreeIterator::Create(LevelDBTransaction* transaction) {
  return make_scoped_ptr(new TreeIterator(transaction));
}

bool LevelDBTransaction::TreeIterator::IsValid() const { return !!*iterator_; }

void LevelDBTransaction::TreeIterator::SeekToLast() {
  iterator_.StartIterGreatest(tree_);
  if (IsValid())
    key_ = (*iterator_)->key;
}

void LevelDBTransaction::TreeIterator::Seek(const StringPiece& target) {
  iterator_.StartIter(tree_, target, TreeType::EQUAL);
  if (!IsValid())
    iterator_.StartIter(tree_, target, TreeType::GREATER);

  if (IsValid())
    key_ = (*iterator_)->key;
}

void LevelDBTransaction::TreeIterator::Next() {
  DCHECK(IsValid());
  ++iterator_;
  if (IsValid()) {
    DCHECK_GE(transaction_->comparator_->Compare((*iterator_)->key, key_), 0);
    key_ = (*iterator_)->key;
  }
}

void LevelDBTransaction::TreeIterator::Prev() {
  DCHECK(IsValid());
  --iterator_;
  if (IsValid()) {
    DCHECK_LT(tree_->abstractor().comparator_->Compare((*iterator_)->key, key_),
              0);
    key_ = (*iterator_)->key;
  }
}

StringPiece LevelDBTransaction::TreeIterator::Key() const {
  DCHECK(IsValid());
  return key_;
}

StringPiece LevelDBTransaction::TreeIterator::Value() const {
  DCHECK(IsValid());
  DCHECK(!IsDeleted());
  return (*iterator_)->value;
}

bool LevelDBTransaction::TreeIterator::IsDeleted() const {
  DCHECK(IsValid());
  return (*iterator_)->deleted;
}

void LevelDBTransaction::TreeIterator::Reset() {
  DCHECK(IsValid());
  iterator_.StartIter(tree_, key_, TreeType::EQUAL);
  DCHECK(IsValid());
}

LevelDBTransaction::TreeIterator::~TreeIterator() {}

LevelDBTransaction::TreeIterator::TreeIterator(LevelDBTransaction* transaction)
    : tree_(&transaction->tree_), transaction_(transaction) {}

scoped_ptr<LevelDBTransaction::TransactionIterator>
LevelDBTransaction::TransactionIterator::Create(
    scoped_refptr<LevelDBTransaction> transaction) {
  return make_scoped_ptr(new TransactionIterator(transaction));
}

LevelDBTransaction::TransactionIterator::TransactionIterator(
    scoped_refptr<LevelDBTransaction> transaction)
    : transaction_(transaction),
      comparator_(transaction_->comparator_),
      tree_iterator_(TreeIterator::Create(transaction_.get())),
      db_iterator_(transaction_->db_->CreateIterator(&transaction_->snapshot_)),
      current_(0),
      direction_(FORWARD),
      tree_changed_(false) {
  transaction_->RegisterIterator(this);
}

LevelDBTransaction::TransactionIterator::~TransactionIterator() {
  transaction_->UnregisterIterator(this);
}

bool LevelDBTransaction::TransactionIterator::IsValid() const {
  return !!current_;
}

void LevelDBTransaction::TransactionIterator::SeekToLast() {
  tree_iterator_->SeekToLast();
  db_iterator_->SeekToLast();
  direction_ = REVERSE;

  HandleConflictsAndDeletes();
  SetCurrentIteratorToLargestKey();
}

void LevelDBTransaction::TransactionIterator::Seek(const StringPiece& target) {
  tree_iterator_->Seek(target);
  db_iterator_->Seek(target);
  direction_ = FORWARD;

  HandleConflictsAndDeletes();
  SetCurrentIteratorToSmallestKey();
}

void LevelDBTransaction::TransactionIterator::Next() {
  DCHECK(IsValid());
  if (tree_changed_)
    RefreshTreeIterator();

  if (direction_ != FORWARD) {
    // Ensure the non-current iterator is positioned after Key().

    LevelDBIterator* non_current = (current_ == db_iterator_.get())
                                       ? tree_iterator_.get()
                                       : db_iterator_.get();

    non_current->Seek(Key());
    if (non_current->IsValid() &&
        !comparator_->Compare(non_current->Key(), Key()))
      non_current->Next();  // Take an extra step so the non-current key is
                            // strictly greater than Key().

    DCHECK(!non_current->IsValid() ||
           comparator_->Compare(non_current->Key(), Key()) > 0);

    direction_ = FORWARD;
  }

  current_->Next();
  HandleConflictsAndDeletes();
  SetCurrentIteratorToSmallestKey();
}

void LevelDBTransaction::TransactionIterator::Prev() {
  DCHECK(IsValid());
  if (tree_changed_)
    RefreshTreeIterator();

  if (direction_ != REVERSE) {
    // Ensure the non-current iterator is positioned before Key().

    LevelDBIterator* non_current = (current_ == db_iterator_.get())
                                       ? tree_iterator_.get()
                                       : db_iterator_.get();

    non_current->Seek(Key());
    if (non_current->IsValid()) {
      // Iterator is at first entry >= Key().
      // Step back once to entry < key.
      // This is why we don't check for the keys being the same before
      // stepping, like we do in Next() above.
      non_current->Prev();
    } else {
      non_current->SeekToLast();  // Iterator has no entries >= Key(). Position
                                  // at last entry.
    }
    DCHECK(!non_current->IsValid() ||
           comparator_->Compare(non_current->Key(), Key()) < 0);

    direction_ = REVERSE;
  }

  current_->Prev();
  HandleConflictsAndDeletes();
  SetCurrentIteratorToLargestKey();
}

StringPiece LevelDBTransaction::TransactionIterator::Key() const {
  DCHECK(IsValid());
  if (tree_changed_)
    RefreshTreeIterator();
  return current_->Key();
}

StringPiece LevelDBTransaction::TransactionIterator::Value() const {
  DCHECK(IsValid());
  if (tree_changed_)
    RefreshTreeIterator();
  return current_->Value();
}

void LevelDBTransaction::TransactionIterator::TreeChanged() {
  tree_changed_ = true;
}

void LevelDBTransaction::TransactionIterator::RefreshTreeIterator() const {
  DCHECK(tree_changed_);

  tree_changed_ = false;

  if (tree_iterator_->IsValid() && tree_iterator_.get() == current_) {
    tree_iterator_->Reset();
    return;
  }

  if (db_iterator_->IsValid()) {
    // There could be new nodes in the tree that we should iterate over.

    if (direction_ == FORWARD) {
      // Try to seek tree iterator to something greater than the db iterator.
      tree_iterator_->Seek(db_iterator_->Key());
      if (tree_iterator_->IsValid() &&
          !comparator_->Compare(tree_iterator_->Key(), db_iterator_->Key()))
        tree_iterator_->Next();  // If equal, take another step so the tree
                                 // iterator is strictly greater.
    } else {
      // If going backward, seek to a key less than the db iterator.
      DCHECK_EQ(REVERSE, direction_);
      tree_iterator_->Seek(db_iterator_->Key());
      if (tree_iterator_->IsValid())
        tree_iterator_->Prev();
    }
  }
}

bool LevelDBTransaction::TransactionIterator::TreeIteratorIsLower() const {
  return comparator_->Compare(tree_iterator_->Key(), db_iterator_->Key()) < 0;
}

bool LevelDBTransaction::TransactionIterator::TreeIteratorIsHigher() const {
  return comparator_->Compare(tree_iterator_->Key(), db_iterator_->Key()) > 0;
}

void LevelDBTransaction::TransactionIterator::HandleConflictsAndDeletes() {
  bool loop = true;

  while (loop) {
    loop = false;

    if (tree_iterator_->IsValid() && db_iterator_->IsValid() &&
        !comparator_->Compare(tree_iterator_->Key(), db_iterator_->Key())) {
      // For equal keys, the tree iterator takes precedence, so move the
      // database iterator another step.
      if (direction_ == FORWARD)
        db_iterator_->Next();
      else
        db_iterator_->Prev();
    }

    // Skip over delete markers in the tree iterator until it catches up with
    // the db iterator.
    if (tree_iterator_->IsValid() && tree_iterator_->IsDeleted()) {
      if (direction_ == FORWARD &&
          (!db_iterator_->IsValid() || TreeIteratorIsLower())) {
        tree_iterator_->Next();
        loop = true;
      } else if (direction_ == REVERSE &&
                 (!db_iterator_->IsValid() || TreeIteratorIsHigher())) {
        tree_iterator_->Prev();
        loop = true;
      }
    }
  }
}

void
LevelDBTransaction::TransactionIterator::SetCurrentIteratorToSmallestKey() {
  LevelDBIterator* smallest = 0;

  if (tree_iterator_->IsValid())
    smallest = tree_iterator_.get();

  if (db_iterator_->IsValid()) {
    if (!smallest ||
        comparator_->Compare(db_iterator_->Key(), smallest->Key()) < 0)
      smallest = db_iterator_.get();
  }

  current_ = smallest;
}

void LevelDBTransaction::TransactionIterator::SetCurrentIteratorToLargestKey() {
  LevelDBIterator* largest = 0;

  if (tree_iterator_->IsValid())
    largest = tree_iterator_.get();

  if (db_iterator_->IsValid()) {
    if (!largest ||
        comparator_->Compare(db_iterator_->Key(), largest->Key()) > 0)
      largest = db_iterator_.get();
  }

  current_ = largest;
}

void LevelDBTransaction::RegisterIterator(TransactionIterator* iterator) {
  DCHECK(iterators_.find(iterator) == iterators_.end());
  iterators_.insert(iterator);
}

void LevelDBTransaction::UnregisterIterator(TransactionIterator* iterator) {
  DCHECK(iterators_.find(iterator) != iterators_.end());
  iterators_.erase(iterator);
}

void LevelDBTransaction::NotifyIteratorsOfTreeChange() {
  for (std::set<TransactionIterator*>::iterator i = iterators_.begin();
       i != iterators_.end();
       ++i) {
    TransactionIterator* transaction_iterator = *i;
    transaction_iterator->TreeChanged();
  }
}

scoped_ptr<LevelDBWriteOnlyTransaction> LevelDBWriteOnlyTransaction::Create(
    LevelDBDatabase* db) {
  return make_scoped_ptr(new LevelDBWriteOnlyTransaction(db));
}

LevelDBWriteOnlyTransaction::LevelDBWriteOnlyTransaction(LevelDBDatabase* db)
    : db_(db), write_batch_(LevelDBWriteBatch::Create()), finished_(false) {}

LevelDBWriteOnlyTransaction::~LevelDBWriteOnlyTransaction() {
  write_batch_->Clear();
}

void LevelDBWriteOnlyTransaction::Remove(const StringPiece& key) {
  DCHECK(!finished_);
  write_batch_->Remove(key);
}

bool LevelDBWriteOnlyTransaction::Commit() {
  DCHECK(!finished_);

  if (!db_->Write(*write_batch_))
    return false;

  finished_ = true;
  write_batch_->Clear();
  return true;
}

}  // namespace content
