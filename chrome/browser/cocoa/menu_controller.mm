// Copyright (c) 2009 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#import "chrome/browser/cocoa/menu_controller.h"

#include "app/l10n_util_mac.h"
#include "app/menus/simple_menu_model.h"
#include "base/logging.h"
#include "base/sys_string_conversions.h"

@interface MenuController(Private)
- (NSMenu*)menuFromModel:(menus::MenuModel*)model;
- (void)addSeparatorToMenu:(NSMenu*)menu
                   atIndex:(int)index;
- (void)addItemToMenu:(NSMenu*)menu
              atIndex:(int)index
            fromModel:(menus::MenuModel*)model
           modelIndex:(int)modelIndex;
@end

@implementation MenuController

- (id)initWithModel:(menus::MenuModel*)model
    useWithPopUpButtonCell:(BOOL)useWithCell {
  if ((self = [super init])) {
    menu_.reset([[self menuFromModel:model] retain]);
    // If this is to be used with a NSPopUpButtonCell, add an item at the 0th
    // position that's empty. Doing it after the menu has been constructed won't
    // complicate creation logic, and since the tags are model indexes, they
    // are unaffected by the extra item.
    if (useWithCell) {
      scoped_nsobject<NSMenuItem> blankItem(
          [[NSMenuItem alloc] initWithTitle:@"" action:nil keyEquivalent:@""]);
      [menu_ insertItem:blankItem atIndex:0];
    }
  }
  return self;
}

// Creates a NSMenu from the given model. If the model has submenus, this can
// be invoked recursively.
- (NSMenu*)menuFromModel:(menus::MenuModel*)model {
  NSMenu* menu = [[[NSMenu alloc] initWithTitle:@""] autorelease];

  // The indices may not always start at zero (the windows system menu is one
  // example where this is used) so just make sure we can handle it.
  // SimpleMenuModel currently always starts at 0.
  int firstItemIndex = model->GetFirstItemIndex(menu);
  DCHECK(firstItemIndex == 0);
  const int count = model->GetItemCount();
  for (int index = firstItemIndex; index < firstItemIndex + count; index++) {
    int modelIndex = index - firstItemIndex;
    if (model->GetTypeAt(modelIndex) == menus::MenuModel::TYPE_SEPARATOR) {
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
              atIndex:(int)index
            fromModel:(menus::MenuModel*)model
           modelIndex:(int)modelIndex {
  NSString* label =
      l10n_util::FixUpWindowsStyleLabel(model->GetLabelAt(modelIndex));
  scoped_nsobject<NSMenuItem> item(
      [[NSMenuItem alloc] initWithTitle:label
                                 action:@selector(itemSelected:)
                          keyEquivalent:@""]);
  menus::MenuModel::ItemType type = model->GetTypeAt(modelIndex);
  if (type == menus::MenuModel::TYPE_SUBMENU) {
    // Recursively build a submenu from the sub-model at this index.
    [item setTarget:nil];
    [item setAction:nil];
    menus::MenuModel* submenuModel = model->GetSubmenuModelAt(modelIndex);
    NSMenu* submenu =
        [self menuFromModel:(menus::SimpleMenuModel*)submenuModel];
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
  }
  [menu insertItem:item atIndex:index];
}

// Called before the menu is to be displayed to update the state (enabled,
// radio, etc) of each item in the menu.
- (BOOL)validateUserInterfaceItem:(id<NSValidatedUserInterfaceItem>)item {
  SEL action = [item action];
  if (action != @selector(itemSelected:))
    return NO;

  NSInteger modelIndex = [item tag];
  menus::MenuModel* model =
      static_cast<menus::MenuModel*>(
          [[(id)item representedObject] pointerValue]);
  DCHECK(model);
  if (model) {
    BOOL checked = model->IsItemCheckedAt(modelIndex);
    DCHECK([(id)item isKindOfClass:[NSMenuItem class]]);
    [(id)item setState:(checked ? NSOnState : NSOffState)];
    if (model->IsLabelDynamicAt(modelIndex)) {
      NSString* label =
          l10n_util::FixUpWindowsStyleLabel(model->GetLabelAt(modelIndex));
      [(id)item setTitle:label];
    }
    return model->IsEnabledAt(modelIndex);
  }
  return NO;
}

// Called when the user chooses a particular menu item. |sender| is the menu
// item chosen.
- (void)itemSelected:(id)sender {
  NSInteger modelIndex = [sender tag];
  menus::MenuModel* model =
      static_cast<menus::MenuModel*>(
          [[sender representedObject] pointerValue]);
  DCHECK(model);
  if (model);
    model->ActivatedAt(modelIndex);
}

- (NSMenu*)menu {
  return menu_.get();
}

@end
