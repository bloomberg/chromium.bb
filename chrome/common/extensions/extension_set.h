// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_EXTENSION_SET_H_
#define CHROME_COMMON_EXTENSIONS_EXTENSION_SET_H_

#include <iterator>
#include <map>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "chrome/common/extensions/extension.h"
#include "url/gurl.h"

// The one true extension container. Extensions are identified by their id.
// Only one extension can be in the set with a given ID.
class ExtensionSet {
 public:
  typedef std::pair<base::FilePath, std::string> ExtensionPathAndDefaultLocale;
  typedef std::map<std::string, scoped_refptr<const extensions::Extension> >
      ExtensionMap;

  // Iteration over the values of the map (given that it's an ExtensionSet,
  // it should iterate like a set iterator).
  class const_iterator :
      public std::iterator<std::input_iterator_tag,
                           scoped_refptr<const extensions::Extension> > {
   public:
    const_iterator();
    const_iterator(const const_iterator& other);
    explicit const_iterator(ExtensionMap::const_iterator it);
    const_iterator& operator++() {
      ++it_;
      return *this;
    }
    const scoped_refptr<const extensions::Extension> operator*() {
      return it_->second;
    }
    const scoped_refptr<const extensions::Extension>* operator->() {
      return &it_->second;
    }
    bool operator!=(const const_iterator& other) { return it_ != other.it_; }
    bool operator==(const const_iterator& other) { return it_ == other.it_; }

   private:
    ExtensionMap::const_iterator it_;
  };

  ExtensionSet();
  ~ExtensionSet();

  size_t size() const;
  bool is_empty() const;

  // Iteration support.
  const_iterator begin() const { return const_iterator(extensions_.begin()); }
  const_iterator end() const { return const_iterator(extensions_.end()); }

  // Returns true if the set contains the specified extension.
  bool Contains(const std::string& id) const;

  // Adds the specified extension to the set. The set becomes an owner. Any
  // previous extension with the same ID is removed.
  // Returns true if there is no previous extension.
  bool Insert(const scoped_refptr<const extensions::Extension>& extension);

  // Copies different items from |extensions| to the current set and returns
  // whether anything changed.
  bool InsertAll(const ExtensionSet& extensions);

  // Removes the specified extension.
  // Returns true if the set contained the specified extnesion.
  bool Remove(const std::string& id);

  // Removes all extensions.
  void Clear();

  // Returns the extension ID, or empty if none. This includes web URLs that
  // are part of an extension's web extent.
  std::string GetExtensionOrAppIDByURL(const GURL& url) const;

  // Returns the Extension, or NULL if none.  This includes web URLs that are
  // part of an extension's web extent.
  // NOTE: This can return NULL if called before UpdateExtensions receives
  // bulk extension data (e.g. if called from
  // EventBindings::HandleContextCreated)
  const extensions::Extension* GetExtensionOrAppByURL(const GURL& url) const;

  // Returns the hosted app whose web extent contains the URL.
  const extensions::Extension* GetHostedAppByURL(const GURL& url) const;

  // Returns a hosted app that contains any URL that overlaps with the given
  // extent, if one exists.
  const extensions::Extension* GetHostedAppByOverlappingWebExtent(
      const extensions::URLPatternSet& extent) const;

  // Returns true if |new_url| is in the extent of the same extension as
  // |old_url|.  Also returns true if neither URL is in an app.
  bool InSameExtent(const GURL& old_url, const GURL& new_url) const;

  // Look up an Extension object by id.
  const extensions::Extension* GetByID(const std::string& id) const;

  // Gets the IDs of all extensions in the set.
  std::set<std::string> GetIDs() const;

  // Returns true if |info| should get extension api bindings and be permitted
  // to make api calls. Note that this is independent of what extension
  // permissions the given extension has been granted.
  bool ExtensionBindingsAllowed(const GURL& url) const;

 private:
  FRIEND_TEST_ALL_PREFIXES(ExtensionSetTest, ExtensionSet);

  ExtensionMap extensions_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionSet);
};

#endif  // CHROME_COMMON_EXTENSIONS_EXTENSION_SET_H_
