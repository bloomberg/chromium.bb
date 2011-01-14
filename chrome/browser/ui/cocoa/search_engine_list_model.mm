// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/search_engine_list_model.h"

#include "base/sys_string_conversions.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/browser/search_engines/template_url_model_observer.h"

NSString* const kSearchEngineListModelChangedNotification =
    @"kSearchEngineListModelChangedNotification";

@interface SearchEngineListModel(Private)
- (void)buildEngineList;
@end

// C++ bridge from TemplateURLModel to our Obj-C model. When it's told about
// model changes, notifies us to rebuild the list.
class SearchEngineObserver : public TemplateURLModelObserver {
 public:
  SearchEngineObserver(SearchEngineListModel* notify)
      : notify_(notify) { }
  virtual ~SearchEngineObserver() { };

 private:
  // TemplateURLModelObserver methods.
  virtual void OnTemplateURLModelChanged() { [notify_ buildEngineList]; }

  SearchEngineListModel* notify_;  // weak, owns us
};

@implementation SearchEngineListModel

// The windows code allows for a NULL |model| and checks for it throughout
// the code, though I'm not sure why. We follow suit.
- (id)initWithModel:(TemplateURLModel*)model {
  if ((self = [super init])) {
    model_ = model;
    if (model_) {
      observer_.reset(new SearchEngineObserver(self));
      model_->Load();
      model_->AddObserver(observer_.get());
      [self buildEngineList];
    }
  }
  return self;
}

- (void)dealloc {
  if (model_)
    model_->RemoveObserver(observer_.get());
  [super dealloc];
}

// Returns an array of NSString's corresponding to the user-visible names of the
// search engines.
- (NSArray*)searchEngines {
  return engines_.get();
}

- (void)setSearchEngines:(NSArray*)engines {
  engines_.reset([engines retain]);

  // Tell anyone who's listening that something has changed so they need to
  // adjust the UI.
  [[NSNotificationCenter defaultCenter]
      postNotificationName:kSearchEngineListModelChangedNotification
                    object:nil];
}

// Walks the model and builds an array of NSStrings to display to the user.
// Assumes there is a non-NULL model.
- (void)buildEngineList {
  scoped_nsobject<NSMutableArray> engines([[NSMutableArray alloc] init]);

  typedef std::vector<const TemplateURL*> TemplateURLs;
  TemplateURLs modelURLs = model_->GetTemplateURLs();
  for (size_t i = 0; i < modelURLs.size(); ++i) {
    if (modelURLs[i]->ShowInDefaultList())
      [engines addObject:base::SysWideToNSString(modelURLs[i]->short_name())];
  }

  [self setSearchEngines:engines.get()];
}

// The index into |-searchEngines| of the current default search engine.
// -1 if there is no default.
- (NSInteger)defaultIndex {
  if (!model_) return -1;

  NSInteger index = 0;
  const TemplateURL* defaultSearchProvider = model_->GetDefaultSearchProvider();
  if (defaultSearchProvider) {
    typedef std::vector<const TemplateURL*> TemplateURLs;
    TemplateURLs urls = model_->GetTemplateURLs();
    for (std::vector<const TemplateURL*>::iterator it = urls.begin();
         it != urls.end(); ++it) {
      const TemplateURL* url = *it;
      // Skip all the URLs not shown on the default list.
      if (!url->ShowInDefaultList())
        continue;
      if (url->id() == defaultSearchProvider->id())
        return index;
      ++index;
    }
  }
  return -1;
}

- (void)setDefaultIndex:(NSInteger)index {
  if (model_) {
    typedef std::vector<const TemplateURL*> TemplateURLs;
    TemplateURLs urls = model_->GetTemplateURLs();
    for (std::vector<const TemplateURL*>::iterator it = urls.begin();
         it != urls.end(); ++it) {
      const TemplateURL* url = *it;
      // Skip all the URLs not shown on the default list.
      if (!url->ShowInDefaultList())
        continue;
      if (0 == index) {
        model_->SetDefaultSearchProvider(url);
        return;
      }
      --index;
    }
    DCHECK(false);
  }
}

// Return TRUE if the default is managed via policy.
- (BOOL)isDefaultManaged {
  return model_->is_default_search_managed();
}
@end
