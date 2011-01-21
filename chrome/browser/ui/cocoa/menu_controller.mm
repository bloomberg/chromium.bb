// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/menu_controller.h"

#include "base/logging.h"
#include "base/sys_string_conversions.h"
#include "skia/ext/skia_utils_mac.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/base/models/accelerator_cocoa.h"
#include "ui/base/models/simple_menu_model.h"

@interface MenuController (Private)
- (NSMenu*)menuFromModel:(ui::MenuModel*)model;
- (void)addSeparatorToMenu:(NSMenu*)menu
                   atIndex:(int)index;
@end

@implementation MenuController

@synthesize model = model_;
@synthesize useWithPopUpButtonCell = useWithPopUpButtonCell_;

- (id)init {
  self = [super init];
  return self;
}

- (id)initWithModel:(ui::MenuModel*)model
    useWithPopUpButtonCell:(BOOL)useWithCell {
  if ((self = [super init])) {
    model_ = model;
    useWithPopUpButtonCell_ = useWithCell;
    [self menu];
  }
  return self;
}

- (void)dealloc {
  model_ = NULL;
  [super dealloc];
}

// Creates a NSMenu from the given model. If the model has submenus, this can
// be invoked recursively.
- (NSMenu*)menuFromModel:(ui::MenuModel*)model {
  NSMenu* menu = [[[NSMenu alloc] initWithTitle:@""] autorelease];

  // The indices may not always start at zero (the windows system menu is one
  // example where this is used) so just make sure we can handle it.
  // SimpleMenuModel currently always starts at 0.
  int firstItemIndex = model->GetFirstItemIndex(menu);
  DCHECK(firstItemIndex == 0);
  const int count = model->GetItemCount();
  for (int index = firstItemIndex; index < firstItemIndex + count; index++) {
    int modelIndex = index - firstItemIndex;
    if (model->GetTypeAt(modelIndex) == ui::MenuModel::TYPE_SEPARATOR) {
      [self addSeparatorToMenu:menu atIndex:index];
    } else {
      [self addItemToMenu:menu atIndex:index fromModel:model
          modelIndex:modelIndex];
    }
  }

  return menu;
}

// Adds a separator item at the given index. As the separator doesn't need
// anything from the model, this method doesn't need the model index as the
// other method below does.
- (void)addSeparatorToMenu:(NSMenu*)menu
                   atIndex:(int)index {
  NSMenuItem* separator = [NSMenuItem separatorItem];
  [menu insertItem:separator atIndex:index];
}

// Adds an item or a hierarchical menu to the item at the |index|,
// associated with the entry in the model indentifed by |modelIndex|.
- (void)addItemToMenu:(NSMenu*)menu
              atIndex:(NSInteger)index
            fromModel:(ui::MenuModel*)model
           modelIndex:(int)modelIndex {
  NSString* label =
      l10n_util::FixUpWindowsStyleLabel(model->GetLabelAt(modelIndex));
  scoped_nsobject<NSMenuItem> item(
      [[NSMenuItem alloc] initWithTitle:label
                                 action:@selector(itemSelected:)
                          keyEquivalent:@""]);

  // If the menu item has an icon, set it.
  SkBitmap skiaIcon;
  if (model->GetIconAt(modelIndex, &skiaIcon) && !skiaIcon.isNull()) {
    NSImage* icon = gfx::SkBitmapToNSImage(skiaIcon);
    if (icon) {
      [item setImage:icon];
    }
  }

  ui::MenuModel::ItemType type = model->GetTypeAt(modelIndex);
  if (type == ui::MenuModel::TYPE_SUBMENU) {
    // Recursively build a submenu from the sub-model at this index.
    [item setTarget:nil];
    [item setAction:nil];
    ui::MenuModel* submenuModel = model->GetSubmenuModelAt(modelIndex);
    NSMenu* submenu =
        [self menuFromModel:(ui::SimpleMenuModel*)submenuModel];
    [item setSubmenu:submenu];
  } else {
    // The MenuModel works on indexes so we can't just set the command id as the
    // tag like we do in other menus. Also set the represented object to be
    // the model so hierarchical menus check the correct index in the correct
    // model. Setting the target to |self| allows this class to participate
    // in validation of the menu items.
    [item setTag:modelIndex];
    [item setTarget:self];
    NSValue* modelObject = [NSValue valueWithPointer:model];
    [item setRepresentedObject:modelObject];  // Retains |modelObject|.
    ui::AcceleratorCocoa accelerator;
    if (model->GetAcceleratorAt(modelIndex, &accelerator)) {
      [item setKeyEquivalent:accelerator.characters()];
      [item setKeyEquivalentModifierMask:accelerator.modifiers()];
    }
  }
  [menu insertItem:item atIndex:index];
}

// Called before the menu is to be displayed to update the state (enabled,
// radio, etc) of each item in the menu. Also will update the title if
// the item is marked as "dynamic".
- (BOOL)validateUserInterfaceItem:(id<NSValidatedUserInterfaceItem>)item {
  SEL action = [item action];
  if (action != @selector(itemSelected:))
    return NO;

  NSInteger modelIndex = [item tag];
  ui::MenuModel* model =
      static_cast<ui::MenuModel*>(
          [[(id)item representedObject] pointerValue]);
  DCHECK(model);
  if (model) {
    BOOL checked = model->IsItemCheckedAt(modelIndex);
    DCHECK([(id)item isKindOfClass:[NSMenuItem class]]);
    [(id)item setState:(checked ? NSOnState : NSOffState)];
    [(id)item setHidden:(!model->IsVisibleAt(modelIndex))];
    if (model->IsItemDynamicAt(modelIndex)) {
      // Update the label and the icon.
      NSString* label =
          l10n_util::FixUpWindowsStyleLabel(model->GetLabelAt(modelIndex));
      [(id)item setTitle:label];
      SkBitmap skiaIcon;
      NSImage* icon = nil;
      if (model->GetIconAt(modelIndex, &skiaIcon) && !skiaIcon.isNull())
        icon = gfx::SkBitmapToNSImage(skiaIcon);
      [(id)item setImage:icon];
    }
    return model->IsEnabledAt(modelIndex);
  }
  return NO;
}

// Called when the user chooses a particular menu item. |sender| is the menu
// item chosen.
- (void)itemSelected:(id)sender {
  NSInteger modelIndex = [sender tag];
  ui::MenuModel* model =
      static_cast<ui::MenuModel*>(
          [[sender representedObject] pointerValue]);
  DCHECK(model);
  if (model)
    model->ActivatedAt(modelIndex);
}

- (NSMenu*)menu {
  if (!menu_ && model_) {
    menu_.reset([[self menuFromModel:model_] retain]);
    [menu_ setDelegate:self];
    // If this is to be used with a NSPopUpButtonCell, add an item at the 0th
    // position that's empty. Doing it after the menu has been constructed won't
    // complicate creation logic, and since the tags are model indexes, they
    // are unaffected by the extra item.
    if (useWithPopUpButtonCell_) {
      scoped_nsobject<NSMenuItem> blankItem(
          [[NSMenuItem alloc] initWithTitle:@"" action:nil keyEquivalent:@""]);
      [menu_ insertItem:blankItem atIndex:0];
    }
  }
  return menu_.get();
}

- (void)menuDidClose:(NSMenu*)menu {
  model_->MenuClosed();
}

@end
