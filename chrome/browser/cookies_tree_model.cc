// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cookies_tree_model.h"

#include <algorithm>
#include <functional>
#include <vector>

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "app/table_model_observer.h"
#include "app/tree_node_model.h"
#include "base/linked_ptr.h"
#include "base/string_util.h"
#include "chrome/browser/net/chrome_url_request_context.h"
#include "chrome/browser/profile.h"
#include "grit/app_resources.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "net/base/cookie_monster.h"
#include "net/base/registry_controlled_domain.h"
#include "net/url_request/url_request_context.h"
#include "third_party/skia/include/core/SkBitmap.h"


///////////////////////////////////////////////////////////////////////////////
// CookieTreeNode, public:

void CookieTreeNode::DeleteStoredObjects() {
  std::for_each(children().begin(),
                children().end(),
                std::mem_fun(&CookieTreeNode::DeleteStoredObjects));
}

CookiesTreeModel* CookieTreeNode::GetModel() const {
  if (GetParent())
    return GetParent()->GetModel();
  else
    return NULL;
}

///////////////////////////////////////////////////////////////////////////////
// CookieTreeCookieNode, public:

CookieTreeCookieNode::CookieTreeCookieNode(
    net::CookieMonster::CookieListPair* cookie)
    : CookieTreeNode(UTF8ToWide(cookie->second.Name())),
      cookie_(cookie) {
}

void CookieTreeCookieNode::DeleteStoredObjects() {
  GetModel()->DeleteCookie(*cookie_);
}

namespace {
// comparison functor, for use in CookieTreeRootNode
class OriginNodeComparator {
 public:
  bool operator() (const CookieTreeNode* lhs,
                   const CookieTreeNode* rhs) {
    // We want to order by registry controlled domain, so we would get
    // google.com, ad.google.com, www.google.com,
    // microsoft.com, ad.microsoft.com. CanonicalizeHost transforms the origins
    // into a form like google.com.www so that string comparisons work.
    return (CanonicalizeHost(lhs->GetTitle()) <
            CanonicalizeHost(rhs->GetTitle()));
  }

 private:
  static std::string CanonicalizeHost(const std::wstring& host_w) {
    // The canonicalized representation makes the registry controlled domain
    // come first, and then adds subdomains in reverse order, e.g.
    // 1.mail.google.com would become google.com.mail.1, and then a standard
    // string comparison works to order hosts by registry controlled domain
    // first. Leading dots are ignored, ".google.com" is the same as
    // "google.com".

    std::string host = WideToUTF8(host_w);
    std::string retval = net::RegistryControlledDomainService::
        GetDomainAndRegistry(host);
    if (!retval.length())  // Is an IP address or other special origin.
      return host;

    std::string::size_type position = host.rfind(retval);

    // The host may be the registry controlled domain, in which case fail fast.
    if (position == 0 || position == std::string::npos)
      return host;

    // If host is www.google.com, retval will contain google.com at this point.
    // Start operating to the left of the registry controlled domain, e.g. in
    // the www.google.com example, start at index 3.
    --position;

    // If position == 0, that means it's a dot; this will be ignored to treat
    // ".google.com" the same as "google.com".
    while (position > 0) {
      retval += std::string(".");
      // Copy up to the next dot. host[position] is a dot so start after it.
      std::string::size_type next_dot = host.rfind(".", position - 1);
      if (next_dot == std::string::npos) {
        retval += host.substr(0, position);
        break;
      }
      retval += host.substr(next_dot + 1, position - (next_dot + 1));
      position = next_dot;
    }
    return retval;
  }
};

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// CookieTreeRootNode, public:
CookieTreeOriginNode* CookieTreeRootNode::GetOrCreateOriginNode(
    const std::wstring& origin) {
  // Strip the trailing dot if it exists.
  std::wstring rewritten_origin = origin;
  if (origin.length() >= 1 && origin[0] == '.')
    rewritten_origin = origin.substr(1);

  CookieTreeOriginNode rewritten_origin_node(rewritten_origin);

  // First see if there is an existing match.
  std::vector<CookieTreeNode*>::iterator origin_node_iterator =
      lower_bound(children().begin(),
                  children().end(),
                  &rewritten_origin_node,
                  OriginNodeComparator());

  if (origin_node_iterator != children().end() && rewritten_origin ==
      (*origin_node_iterator)->GetTitle())
    return static_cast<CookieTreeOriginNode*>(*origin_node_iterator);
  // Node doesn't exist, create a new one and insert it into the (ordered)
  // children.
  CookieTreeOriginNode* retval = new CookieTreeOriginNode(rewritten_origin);
  DCHECK(model_);
  model_->Add(this, (origin_node_iterator - children().begin()), retval);
  return retval;
}

///////////////////////////////////////////////////////////////////////////////
// CookieTreeOriginNode, public:

CookieTreeCookiesNode* CookieTreeOriginNode::GetOrCreateCookiesNode() {
  if (cookies_child_)
    return cookies_child_;
  // need to make a Cookies node, add it to the tree, and return it
  CookieTreeCookiesNode* retval = new CookieTreeCookiesNode;
  GetModel()->Add(this, 0, retval);
  cookies_child_ = retval;
  return retval;
}

///////////////////////////////////////////////////////////////////////////////
// CookieTreeCookiesNode, public:

CookieTreeCookiesNode::CookieTreeCookiesNode()
    : CookieTreeNode(l10n_util::GetString(IDS_COOKIES_COOKIES)) {}


void CookieTreeCookiesNode::AddCookieNode(
    CookieTreeCookieNode* new_child) {
  std::vector<CookieTreeNode*>::iterator cookie_iterator =
      lower_bound(children().begin(),
                  children().end(),
                  new_child,
                  CookieTreeCookieNode::CookieNodeComparator());
  GetModel()->Add(this, (cookie_iterator - children().begin()), new_child);
}

///////////////////////////////////////////////////////////////////////////////
// CookieTreeCookieNode, private

bool CookieTreeCookieNode::CookieNodeComparator::operator() (
    const CookieTreeNode* lhs, const CookieTreeNode* rhs) {
  return (static_cast<const CookieTreeCookieNode*>(lhs)->
          cookie_->second.Name() <
          static_cast<const CookieTreeCookieNode*>(rhs)->
          cookie_->second.Name());
}

///////////////////////////////////////////////////////////////////////////////
// CookiesTreeModel, public:

CookiesTreeModel::CookiesTreeModel(Profile* profile)
    : ALLOW_THIS_IN_INITIALIZER_LIST(TreeNodeModel<CookieTreeNode>(
          new CookieTreeRootNode(this))),
      profile_(profile) {
  LoadCookies();
}

///////////////////////////////////////////////////////////////////////////////
// CookiesTreeModel, TreeModel methods (public):

// TreeModel methods:
// Returns the set of icons for the nodes in the tree. You only need override
// this if you don't want to use the default folder icons.
void CookiesTreeModel::GetIcons(std::vector<SkBitmap>* icons) {
  icons->push_back(*ResourceBundle::GetSharedInstance().GetBitmapNamed(
      IDR_DEFAULT_FAVICON));
  icons->push_back(*ResourceBundle::GetSharedInstance().GetBitmapNamed(
      IDR_COOKIE_ICON));
}

// Returns the index of the icon to use for |node|. Return -1 to use the
// default icon. The index is relative to the list of icons returned from
// GetIcons.
int CookiesTreeModel::GetIconIndex(TreeModelNode* node) {
  CookieTreeNode* ct_node = static_cast<CookieTreeNode*>(node);
  switch (ct_node->GetDetailedInfo().node_type) {
    case CookieTreeNode::DetailedInfo::TYPE_ORIGIN:
      return ORIGIN;
      break;
    case CookieTreeNode::DetailedInfo::TYPE_COOKIE:
      return COOKIE;
      break;
    default:
      return -1;
  }
}

void CookiesTreeModel::LoadCookies() {
  LoadCookiesWithFilter(L"");
}

void CookiesTreeModel::LoadCookiesWithFilter(const std::wstring& filter) {
  // mmargh mmargh mmargh!

  // Since we are running on the UI thread don't call GetURLRequestContext().
  net::CookieMonster* cookie_monster =
      profile_->GetRequestContext()->GetCookieStore()->GetCookieMonster();

  all_cookies_ = cookie_monster->GetAllCookies();
  CookieTreeRootNode* root = static_cast<CookieTreeRootNode*>(GetRoot());
  for (CookieList::iterator it = all_cookies_.begin();
       it != all_cookies_.end();
       ++it) {
    // Get the origin cookie
    if (!filter.size() ||
        (UTF8ToWide(it->first).find(filter) != std::wstring::npos)) {
      CookieTreeOriginNode* origin =
          root->GetOrCreateOriginNode(UTF8ToWide(it->first));
      CookieTreeCookiesNode* cookies_node = origin->GetOrCreateCookiesNode();
      CookieTreeCookieNode* new_cookie = new CookieTreeCookieNode(&*it);
      cookies_node->AddCookieNode(new_cookie);
    }
  }
}

void CookiesTreeModel::DeleteCookie(
    const net::CookieMonster::CookieListPair& cookie) {
  // notify CookieMonster that we should delete this cookie
  // Since we are running on the UI thread don't call GetURLRequestContext().
  net::CookieMonster* monster =
      profile_->GetRequestContext()->GetCookieStore()->GetCookieMonster();
  // We have stored a copy of all the cookies in the model, and our model is
  // never re-calculated. Thus, we just need to delete the nodes from our
  // model, and tell CookieMonster to delete the cookies. We can keep the
  // vector storing the cookies in-tact and not delete from there (that would
  // invalidate our pointers), and the fact that it contains semi out-of-date
  // data is not problematic as we don't re-build the model based on that.
  monster->DeleteCookie(cookie.first, cookie.second, true);
}

void CookiesTreeModel::DeleteAllCookies() {
  CookieTreeNode* root = GetRoot();
  root->DeleteStoredObjects();
  int num_children = root->GetChildCount();
  for (int i = num_children - 1; i >= 0; --i)
    delete Remove(root, i);
  LoadCookies();
  NotifyObserverTreeNodeChanged(root);
}

void CookiesTreeModel::DeleteCookieNode(CookieTreeNode* cookie_node) {
  cookie_node->DeleteStoredObjects();
  // find the parent and index
  CookieTreeNode* parent_node = cookie_node->GetParent();
  int cookie_node_index = parent_node->IndexOfChild(cookie_node);
  delete Remove(parent_node, cookie_node_index);
}

void CookiesTreeModel::UpdateSearchResults(const std::wstring& filter) {
  CookieTreeNode* root = GetRoot();
  int num_children = root->GetChildCount();
  for (int i = num_children - 1; i >= 0; --i)
    delete Remove(root, i);
  LoadCookiesWithFilter(filter);
  NotifyObserverTreeNodeChanged(root);
}
