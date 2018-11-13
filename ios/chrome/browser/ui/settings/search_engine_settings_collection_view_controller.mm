// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/search_engine_settings_collection_view_controller.h"

#include <memory>

#include "base/mac/foundation_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/sys_string_conversions.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_service_observer.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/search_engines/search_engine_observer_bridge.h"
#include "ios/chrome/browser/search_engines/template_url_service_factory.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_model.h"
#import "ios/chrome/browser/ui/settings/cells/settings_text_item.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/third_party/material_components_ios/src/components/CollectionCells/src/MaterialCollectionCells.h"
#import "ios/third_party/material_components_ios/src/components/Palettes/src/MaterialPalettes.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierPriorSearchEngines = kSectionIdentifierEnumZero,
  SectionIdentifierCustomSearchEngines,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypePriorSearchEnginesEngine = kItemTypeEnumZero,
  ItemTypeCustomSearchEnginesEngineHeader,
  ItemTypeCustomSearchEnginesEngine,
};

constexpr base::TimeDelta kMaxVisitAge = base::TimeDelta::FromDays(2);
const size_t kMaxcustomSearchEngines = 3;
const char kUmaSelectDefaultSearchEngine[] =
    "Search.iOS.SelectDefaultSearchEngine";

}  // namespace

@interface SearchEngineSettingsCollectionViewController ()<
    SearchEngineObserving>
@end

@implementation SearchEngineSettingsCollectionViewController {
  TemplateURLService* _templateURLService;  // weak
  std::unique_ptr<SearchEngineObserverBridge> _observer;
  // Prevent unnecessary notifications when we write to the setting.
  BOOL _updatingBackend;
  // The first list in the page which contains prepopulted search engines and
  // search engines that are created by policy, and possibly one custom search
  // engine if it's selected as default search engine.
  std::vector<TemplateURL*> _priorSearchEngines;
  // The second list in the page which contains all remaining custom search
  // engines.
  std::vector<TemplateURL*> _customSearchEngines;
}

#pragma mark Initialization

- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState {
  DCHECK(browserState);
  UICollectionViewLayout* layout = [[MDCCollectionViewFlowLayout alloc] init];
  self =
      [super initWithLayout:layout style:CollectionViewControllerStyleAppBar];
  if (self) {
    _templateURLService =
        ios::TemplateURLServiceFactory::GetForBrowserState(browserState);
    _observer =
        std::make_unique<SearchEngineObserverBridge>(self, _templateURLService);
    _templateURLService->Load();
    [self setTitle:l10n_util::GetNSString(IDS_IOS_SEARCH_ENGINE_SETTING_TITLE)];
    [self setCollectionViewAccessibilityIdentifier:@"Search Engine"];
    // TODO(crbug.com/764578): -loadModel should not be called from
    // initializer. A possible fix is to move this call to -viewDidLoad.
    [self loadModel];
  }
  return self;
}

- (void)loadModel {
  [super loadModel];
  CollectionViewModel* model = self.collectionViewModel;
  [self loadSearchEngines];

  // Add prior search engines.
  if (_priorSearchEngines.size() > 0) {
    [model addSectionWithIdentifier:SectionIdentifierPriorSearchEngines];

    for (TemplateURL* url : _priorSearchEngines) {
      SettingsTextItem* engine = [[SettingsTextItem alloc]
          initWithType:ItemTypePriorSearchEnginesEngine];
      [engine setText:base::SysUTF16ToNSString(url->short_name())];
      if (url == _templateURLService->GetDefaultSearchProvider()) {
        [engine setAccessoryType:MDCCollectionViewCellAccessoryCheckmark];
      }
      [model addItem:engine
          toSectionWithIdentifier:SectionIdentifierPriorSearchEngines];
    }
  }

  // Add custom search engines.
  if (_customSearchEngines.size() > 0) {
    [model addSectionWithIdentifier:SectionIdentifierCustomSearchEngines];

    SettingsTextItem* header = [[SettingsTextItem alloc]
        initWithType:ItemTypeCustomSearchEnginesEngineHeader];
    header.text = l10n_util::GetNSString(
        IDS_IOS_SEARCH_ENGINE_SETTING_CUSTOM_SECTION_HEADER);
    header.textColor = [[MDCPalette greyPalette] tint500];
    [model setHeader:header
        forSectionWithIdentifier:SectionIdentifierCustomSearchEngines];

    for (TemplateURL* url : _customSearchEngines) {
      SettingsTextItem* engine = [[SettingsTextItem alloc]
          initWithType:ItemTypeCustomSearchEnginesEngine];
      [engine setText:base::SysUTF16ToNSString(url->short_name())];
      if (url == _templateURLService->GetDefaultSearchProvider()) {
        [engine setAccessoryType:MDCCollectionViewCellAccessoryCheckmark];
      }
      [model addItem:engine
          toSectionWithIdentifier:SectionIdentifierCustomSearchEngines];
    }
  }
}

#pragma mark UICollectionViewDelegate

- (void)collectionView:(UICollectionView*)collectionView
    didSelectItemAtIndexPath:(NSIndexPath*)indexPath {
  [super collectionView:collectionView didSelectItemAtIndexPath:indexPath];
  CollectionViewModel* model = self.collectionViewModel;

  // Only handle taps on search engine items.
  CollectionViewItem* selectedItem = [model itemAtIndexPath:indexPath];
  if (selectedItem.type != ItemTypePriorSearchEnginesEngine &&
      selectedItem.type != ItemTypeCustomSearchEnginesEngine) {
    return;
  }

  // Do nothing if the tapped engine was already the default.
  SettingsTextItem* selectedTextItem =
      base::mac::ObjCCastStrict<SettingsTextItem>(selectedItem);
  if (selectedTextItem.accessoryType ==
      MDCCollectionViewCellAccessoryCheckmark) {
    return;
  }

  NSMutableArray* modifiedItems = [NSMutableArray array];

  // Iterate through the engines and remove the checkmark from any that have it.
  if ([model
          hasSectionForSectionIdentifier:SectionIdentifierPriorSearchEngines]) {
    for (CollectionViewItem* item in
         [model itemsInSectionWithIdentifier:
                    SectionIdentifierPriorSearchEngines]) {
      if (item.type != ItemTypePriorSearchEnginesEngine) {
        continue;
      }
      SettingsTextItem* textItem =
          base::mac::ObjCCastStrict<SettingsTextItem>(item);
      if (textItem.accessoryType == MDCCollectionViewCellAccessoryCheckmark) {
        textItem.accessoryType = MDCCollectionViewCellAccessoryNone;
        [modifiedItems addObject:textItem];
      }
    }
  }
  if ([model hasSectionForSectionIdentifier:
                 SectionIdentifierCustomSearchEngines]) {
    for (CollectionViewItem* item in
         [model itemsInSectionWithIdentifier:
                    SectionIdentifierCustomSearchEngines]) {
      if (item.type != ItemTypeCustomSearchEnginesEngine) {
        continue;
      }
      SettingsTextItem* textItem =
          base::mac::ObjCCastStrict<SettingsTextItem>(item);
      if (textItem.accessoryType == MDCCollectionViewCellAccessoryCheckmark) {
        textItem.accessoryType = MDCCollectionViewCellAccessoryNone;
        [modifiedItems addObject:textItem];
      }
    }
  }

  // Show the checkmark on the new default engine.
  SettingsTextItem* newDefaultEngine =
      base::mac::ObjCCastStrict<SettingsTextItem>(
          [model itemAtIndexPath:indexPath]);
  newDefaultEngine.accessoryType = MDCCollectionViewCellAccessoryCheckmark;
  [modifiedItems addObject:newDefaultEngine];

  // Set the new engine as the default.
  if (selectedItem.type == ItemTypePriorSearchEnginesEngine)
    [self setDefaultToPriorSearchEngineAtIndex:
              [model indexInItemTypeForIndexPath:indexPath]];
  else
    [self setDefaultToCustomSearchEngineAtIndex:
              [model indexInItemTypeForIndexPath:indexPath]];

  [self reconfigureCellsForItems:modifiedItems];
}

#pragma mark Internal methods

// Loads all TemplateURLs from TemplateURLService and classifies them into
// |_priorSearchEngines| and |_customSearchEngines|. If a TemplateURL is
// prepopulated, created by policy or the default search engine, it will get
// into the first list, otherwise the second list.
- (void)loadSearchEngines {
  std::vector<TemplateURL*> urls = _templateURLService->GetTemplateURLs();
  _priorSearchEngines.clear();
  _priorSearchEngines.reserve(urls.size());
  _customSearchEngines.clear();
  _customSearchEngines.reserve(urls.size());

  // Classify TemplateURLs.
  for (TemplateURL* url : urls) {
    if (_templateURLService->IsPrepopulatedOrCreatedByPolicy(url) ||
        url == _templateURLService->GetDefaultSearchProvider())
      _priorSearchEngines.push_back(url);
    else
      _customSearchEngines.push_back(url);
  }
  // Sort |fixedCutomeSearchEngines_| by TemplateURL's prepopulate_id. If
  // prepopulated_id == 0, it's a custom search engine and should be put in the
  // end of the list.
  std::sort(_priorSearchEngines.begin(), _priorSearchEngines.end(),
            [](const TemplateURL* lhs, const TemplateURL* rhs) {
              if (lhs->prepopulate_id() == 0)
                return false;
              if (rhs->prepopulate_id() == 0)
                return true;
              return lhs->prepopulate_id() < rhs->prepopulate_id();
            });

  // Partially sort |_customSearchEngines| by TemplateURL's last_visited time.
  auto begin = _customSearchEngines.begin();
  auto end = _customSearchEngines.end();
  auto pivot =
      begin + std::min(kMaxcustomSearchEngines, _customSearchEngines.size());
  std::partial_sort(begin, pivot, end,
                    [](const TemplateURL* lhs, const TemplateURL* rhs) {
                      return lhs->last_visited() > rhs->last_visited();
                    });

  // Keep the search engines visited within |kMaxVisitAge| and erase others.
  const base::Time cutoff = base::Time::Now() - kMaxVisitAge;
  auto cutBegin = std::find_if(begin, pivot, [cutoff](const TemplateURL* url) {
    return url->last_visited() < cutoff;
  });
  _customSearchEngines.erase(cutBegin, end);
}

// Sets the search engine at |index| in prior section as default search engine.
- (void)setDefaultToPriorSearchEngineAtIndex:(NSUInteger)index {
  DCHECK_GE(index, 0U);
  DCHECK_LT(index, _priorSearchEngines.size());
  _updatingBackend = YES;
  _templateURLService->SetUserSelectedDefaultSearchProvider(
      _priorSearchEngines[index]);
  [self recordUmaOfDefaultSearchEngine];
  _updatingBackend = NO;
}

// Sets the search engine at |index| in custom section as default search engine.
- (void)setDefaultToCustomSearchEngineAtIndex:(NSUInteger)index {
  DCHECK_GE(index, 0U);
  DCHECK_LT(index, _customSearchEngines.size());
  _updatingBackend = YES;
  _templateURLService->SetUserSelectedDefaultSearchProvider(
      _customSearchEngines[index]);
  [self recordUmaOfDefaultSearchEngine];
  _updatingBackend = NO;
}

// Records the type of the selected default search engine.
- (void)recordUmaOfDefaultSearchEngine {
  UMA_HISTOGRAM_ENUMERATION(
      kUmaSelectDefaultSearchEngine,
      _templateURLService->GetDefaultSearchProvider()->GetEngineType(
          _templateURLService->search_terms_data()),
      SEARCH_ENGINE_MAX);
}

- (void)searchEngineChanged {
  if (!_updatingBackend)
    [self reloadData];
}

@end
