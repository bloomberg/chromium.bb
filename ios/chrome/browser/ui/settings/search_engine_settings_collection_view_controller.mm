// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/search_engine_settings_collection_view_controller.h"

#include <memory>

#import "base/ios/weak_nsobject.h"
#include "base/mac/foundation_util.h"
#import "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_service_observer.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/search_engines/template_url_service_factory.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_text_item.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_model.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/third_party/material_components_ios/src/components/CollectionCells/src/MaterialCollectionCells.h"
#include "ui/base/l10n/l10n_util_mac.h"

@interface SearchEngineSettingsCollectionViewController ()
- (void)onChange;
@end

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierSearchEngines = kSectionIdentifierEnumZero,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeSearchEnginesEngine = kItemTypeEnumZero,
};

// Observer used to reload the Search Engine collection view once the
// TemplateURLService changes, either on first load or due to a
// policy change.
class SearchEngineObserver : public TemplateURLServiceObserver {
 public:
  SearchEngineObserver(SearchEngineSettingsCollectionViewController* owner,
                       TemplateURLService* urlService);
  ~SearchEngineObserver() override;
  void OnTemplateURLServiceChanged() override;

 private:
  base::WeakNSObject<SearchEngineSettingsCollectionViewController> owner_;
  TemplateURLService* templateURLService_;  // weak
};

SearchEngineObserver::SearchEngineObserver(
    SearchEngineSettingsCollectionViewController* owner,
    TemplateURLService* urlService)
    : owner_(owner), templateURLService_(urlService) {
  templateURLService_->AddObserver(this);
}

SearchEngineObserver::~SearchEngineObserver() {
  templateURLService_->RemoveObserver(this);
}

void SearchEngineObserver::OnTemplateURLServiceChanged() {
  base::scoped_nsobject<SearchEngineSettingsCollectionViewController>
      strongOwner([owner_.get() retain]);
  [strongOwner onChange];
}

}  // namespace

@implementation SearchEngineSettingsCollectionViewController {
  TemplateURLService* templateURLService_;  // weak
  std::unique_ptr<SearchEngineObserver> observer_;
  // Prevent unnecessary notifications when we write to the setting.
  BOOL updatingBackend_;
}

#pragma mark Initialization

- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState {
  DCHECK(browserState);
  self = [super initWithStyle:CollectionViewControllerStyleAppBar];
  if (self) {
    templateURLService_ =
        ios::TemplateURLServiceFactory::GetForBrowserState(browserState);
    observer_.reset(new SearchEngineObserver(self, templateURLService_));
    templateURLService_->Load();
    [self setTitle:l10n_util::GetNSString(IDS_IOS_SEARCH_ENGINE_SETTING_TITLE)];
    [self setCollectionViewAccessibilityIdentifier:@"Search Engine"];
    [self loadModel];
  }
  return self;
}

- (void)loadModel {
  [super loadModel];
  CollectionViewModel* model = self.collectionViewModel;
  NSArray* values = [self allValues];

  // Do not add any sections if there are no search engines.
  if (![values count]) {
    return;
  }

  [model addSectionWithIdentifier:SectionIdentifierSearchEngines];
  for (NSUInteger i = 0; i < values.count; i++) {
    NSString* value = values[i];
    BOOL checked = [value isEqualToString:[self currentValue]];

    base::scoped_nsobject<CollectionViewTextItem> engine(
        [[CollectionViewTextItem alloc]
            initWithType:ItemTypeSearchEnginesEngine]);
    [engine setText:value];
    if (checked) {
      [engine setAccessoryType:MDCCollectionViewCellAccessoryCheckmark];
    }
    [model addItem:engine
        toSectionWithIdentifier:SectionIdentifierSearchEngines];
  }
}

#pragma mark UICollectionViewDelegate

- (void)collectionView:(UICollectionView*)collectionView
    didSelectItemAtIndexPath:(NSIndexPath*)indexPath {
  [super collectionView:collectionView didSelectItemAtIndexPath:indexPath];
  CollectionViewModel* model = self.collectionViewModel;

  // Only handle taps on search engine items.
  CollectionViewItem* selectedItem = [model itemAtIndexPath:indexPath];
  if (selectedItem.type != ItemTypeSearchEnginesEngine) {
    return;
  }

  // Do nothing if the tapped engine was already the default.
  CollectionViewTextItem* selectedTextItem =
      base::mac::ObjCCastStrict<CollectionViewTextItem>(selectedItem);
  if (selectedTextItem.accessoryType ==
      MDCCollectionViewCellAccessoryCheckmark) {
    return;
  }

  // Iterate through the engines and remove the checkmark from any that have it.
  NSMutableArray* modifiedItems = [NSMutableArray array];
  for (CollectionViewItem* item in
       [model itemsInSectionWithIdentifier:SectionIdentifierSearchEngines]) {
    if (item.type != ItemTypeSearchEnginesEngine) {
      continue;
    }

    CollectionViewTextItem* textItem =
        base::mac::ObjCCastStrict<CollectionViewTextItem>(item);
    if (textItem.accessoryType == MDCCollectionViewCellAccessoryCheckmark) {
      textItem.accessoryType = MDCCollectionViewCellAccessoryNone;
      [modifiedItems addObject:textItem];
    }
  }

  // Show the checkmark on the new default engine.
  CollectionViewTextItem* newDefaultEngine =
      base::mac::ObjCCastStrict<CollectionViewTextItem>(
          [model itemAtIndexPath:indexPath]);
  newDefaultEngine.accessoryType = MDCCollectionViewCellAccessoryCheckmark;
  [modifiedItems addObject:newDefaultEngine];

  // Set the new engine as the default.
  [self setValueFromIndex:[model indexInItemTypeForIndexPath:indexPath]];

  [self reconfigureCellsForItems:modifiedItems
         inSectionWithIdentifier:SectionIdentifierSearchEngines];
}

#pragma mark Internal methods

- (NSArray*)allValues {
  std::vector<TemplateURL*> urls = templateURLService_->GetTemplateURLs();
  NSMutableArray* items = [NSMutableArray arrayWithCapacity:urls.size()];
  for (std::vector<TemplateURL*>::const_iterator iter = urls.begin();
       iter != urls.end(); ++iter) {
    [items addObject:base::SysUTF16ToNSString((*iter)->short_name())];
  }
  return items;
}

- (NSString*)currentValue {
  return base::SysUTF16ToNSString(
      templateURLService_->GetDefaultSearchProvider()->short_name());
}

- (void)setValueFromIndex:(NSUInteger)index {
  std::vector<TemplateURL*> urls = templateURLService_->GetTemplateURLs();
  DCHECK_GE(index, 0U);
  DCHECK_LT(index, urls.size());
  updatingBackend_ = YES;
  templateURLService_->SetUserSelectedDefaultSearchProvider(urls[index]);
  updatingBackend_ = NO;
}

- (void)onChange {
  if (!updatingBackend_)
    [self reloadData];
}

@end
