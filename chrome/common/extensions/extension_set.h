// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_EXTENSION_SET_H_
#define CHROME_COMMON_EXTENSIONS_EXTENSION_SET_H_
#pragma once

#include <iterator>
#include <map>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "chrome/common/extensions/extension.h"
#include "googleurl/src/gurl.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSecurityOrigin.h"

class ExtensionURLInfo {
 public:
  // The extension system uses both a document's origin and its URL to
  // grant permissions. Ideally, we would use only the origin, but because
  // the web extent of a hosted app can be less than an entire origin, we
  // take the URL into account as well
  ExtensionURLInfo(WebKit::WebSecurityOrigin origin, const GURL& url);

  // WARNING! Using this constructor can miss important security checks if
  //          you're trying to find a running extension. For example, if the
  //          URL in question is being rendered inside an iframe sandbox, then
  //          we might incorrectly grant it access to powerful extension APIs.
  explicit ExtensionURLInfo(const GURL& url);

  const WebKit::WebSecurityOrigin& origin() const { return origin_; }
  const GURL& url() const { return url_; }

 private:
  WebKit::WebSecurityOrigin origin_;
  GURL url_;
};

// The one true extension container. Extensions are identified by their id.
// Only one extension can be in the set with a given ID.
class ExtensionSet {
 public:
  typedef std::pair<FilePath, std::string> ExtensionPathAndDefaultLocale;
  typedef std::map<std::string, scoped_refptr<const Extension> > ExtensionMap;

  // Iteration over the values of the map (given that it's an ExtensionSet,
  // it should iterate like a set iterator).
  class const_iterator :
      public std::iterator<std::input_iterator_tag,
                           scoped_refptr<const Extension> > {
   public:
    const_iterator() {}
    explicit const_iterator(ExtensionMap::const_iterator it) :
        it_(it) {}
    const_iterator& operator++() {
      ++it_;
      return *this;
    }
    const scoped_refptr<const Extension> operator*() {
      return it_->second;
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
  void Insert(const scoped_refptr<const Extension>& extension);

  // Removes the specified extension.
  void Remove(const std::string& id);

  // Removes all extensions.
  void Clear();

  // Returns the extension ID, or empty if none. This includes web URLs that
  // are part of an extension's web extent.
  std::string GetIDByURL(const ExtensionURLInfo& info) const;

  // Returns the Extension, or NULL if none.  This includes web URLs that are
  // part of an extension's web extent.
  // NOTE: This can return NULL if called before UpdateExtensions receives
  // bulk extension data (e.g. if called from
  // EventBindings::HandleContextCreated)
  const Extension* GetByURL(const ExtensionURLInfo& info) const;

  // Returns true if |new_url| is in the extent of the same extension as
  // |old_url|.  Also returns true if neither URL is in an app.
  bool InSameExtent(const GURL& old_url, const GURL& new_url) const;

  // Look up an Extension object by id.
  const Extension* GetByID(const std::string& id) const;

  // Returns true if |info| should get extension api bindings and be permitted
  // to make api calls. Note that this is independent of what extension
  // permissions the given extension has been granted.
  bool ExtensionBindingsAllowed(const ExtensionURLInfo& info) const;

 private:
  FRIEND_TEST_ALL_PREFIXES(ExtensionSetTest, ExtensionSet);

  ExtensionMap extensions_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionSet);
};

#endif  // CHROME_COMMON_EXTENSIONS_EXTENSION_SET_H_
