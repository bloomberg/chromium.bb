// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/util/label_observer.h"

#import <objc/runtime.h>

#import "base/ios/weak_nsobject.h"
#import "base/mac/scoped_block.h"
#import "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"

namespace {
// The key under which LabelObservers are associated with their labels.
const void* const kLabelObserverKey = &kLabelObserverKey;
// Attempts to convert |value| to a string.
NSString* GetStringValue(id value) {
  if ([value isKindOfClass:[NSString class]])
    return static_cast<NSString*>(value);
  if ([value respondsToSelector:@selector(string)])
    return [value performSelector:@selector(string)];
  return nil;
}
}

@interface LabelObserver () {
  // The label being observed.
  base::WeakNSObject<UILabel> _label;
  // Arrays used to store registered actions.
  base::scoped_nsobject<NSMutableArray> _styleActions;
  base::scoped_nsobject<NSMutableArray> _layoutActions;
  base::scoped_nsobject<NSMutableArray> _textActions;
}

// Whether or not observer actions are currently being executed.  This is used
// to prevent infinite loops caused by a LinkObserverAction updating a
// property on |_label|.
@property(nonatomic, assign, getter=isRespondingToKVO) BOOL respondingToKVO;

// Initializes a LabelObserver that observes |label|.
- (instancetype)initWithLabel:(UILabel*)label NS_DESIGNATED_INITIALIZER;

// Performs all LabelObserverActions in |actions|.
- (void)performActions:(NSArray*)actions;

// Takes |_label|'s values for each key from |styleKeys| and uses them to
// construct a uniformly attributed value to use for |_label|'s attributedText.
- (void)resetLabelAttributes;

@end

// Properties of UILabel that, when changed, will cause the label's attributed
// text to change.
static NSSet* styleKeys;
// Properties of UILabel that invalidate the layout of the label if they change.
static NSSet* layoutKeys;
// Properties of UILabel that may invalidate the text of the label if they
// change.
static NSSet* textKeys;

@implementation LabelObserver

@synthesize respondingToKVO = _respondingToKVO;

+ (void)initialize {
  if (self == [LabelObserver class]) {
    styleKeys = [[NSSet alloc] initWithArray:@[
      @"font", @"textColor", @"textAlignment", @"lineBreakMode", @"shadowColor",
      @"shadowOffset"
    ]];
    layoutKeys = [[NSSet alloc]
        initWithArray:@[ @"bounds", @"frame", @"superview", @"center" ]];
    textKeys = [[NSSet alloc] initWithArray:@[ @"text", @"attributedText" ]];
  }
}

- (instancetype)initWithLabel:(UILabel*)label {
  if ((self = [super init])) {
    DCHECK(label);
    _label.reset(label);
    for (NSSet* keySet in @[ styleKeys, layoutKeys, textKeys ]) {
      for (NSString* key in keySet) {
        [_label addObserver:self
                 forKeyPath:key
                    options:NSKeyValueObservingOptionNew
                    context:nullptr];
      }
    }
    [self resetLabelAttributes];
  }
  return self;
}

- (void)dealloc {
  for (NSSet* keySet in @[ styleKeys, layoutKeys, textKeys ]) {
    for (NSString* key in keySet) {
      [_label removeObserver:self forKeyPath:key];
    }
  }
  [super dealloc];
}

#pragma mark - Public interface

+ (instancetype)observerForLabel:(UILabel*)label {
  if (!label)
    return nil;
  id observer = objc_getAssociatedObject(label, kLabelObserverKey);
  if (!observer) {
    observer = [[LabelObserver alloc] initWithLabel:label];
    objc_setAssociatedObject(label, kLabelObserverKey, observer,
                             OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    [observer release];
  }
  return observer;
}

- (void)addStyleChangedAction:(LabelObserverAction)action {
  DCHECK(action);
  if (!_styleActions)
    _styleActions.reset([[NSMutableArray alloc] init]);
  base::mac::ScopedBlock<LabelObserverAction> actionCopy([action copy]);
  [_styleActions addObject:actionCopy];
}

- (void)addLayoutChangedAction:(LabelObserverAction)action {
  DCHECK(action);
  if (!_layoutActions)
    _layoutActions.reset([[NSMutableArray alloc] init]);
  base::mac::ScopedBlock<LabelObserverAction> actionCopy([action copy]);
  [_layoutActions addObject:actionCopy];
}

- (void)addTextChangedAction:(LabelObserverAction)action {
  DCHECK(action);
  if (!_textActions)
    _textActions.reset([[NSMutableArray alloc] init]);
  base::mac::ScopedBlock<LabelObserverAction> actionCopy([action copy]);
  [_textActions addObject:actionCopy];
}

#pragma mark -

- (void)performActions:(NSArray*)actions {
  for (LabelObserverAction action in actions)
    action(_label);
}

- (void)resetLabelAttributes {
  if ([_label attributedText] || ![_label text])
    return;
  NSMutableDictionary* labelStyle =
      [NSMutableDictionary dictionaryWithCapacity:styleKeys.count];
  for (NSString* property in styleKeys)
    labelStyle[property] = [_label valueForKey:property];
  base::scoped_nsobject<NSAttributedString> attributedText(
      [[NSAttributedString alloc] initWithString:[_label text]]);
  [_label setAttributedText:attributedText];
  for (NSString* property in styleKeys)
    [_label setValue:labelStyle[property] forKey:property];
}

- (void)observeValueForKeyPath:(NSString*)key
                      ofObject:(id)object
                        change:(NSDictionary*)change
                       context:(void*)context {
  if (self.respondingToKVO)
    return;
  self.respondingToKVO = YES;
  DCHECK_EQ(object, _label.get());
  if ([styleKeys containsObject:key]) {
    [self performActions:_styleActions];
  } else if ([layoutKeys containsObject:key]) {
    [self performActions:_layoutActions];
  } else if ([textKeys containsObject:key]) {
    NSString* oldText = GetStringValue(change[NSKeyValueChangeOldKey]);
    NSString* newText = GetStringValue(change[NSKeyValueChangeNewKey]);
    if (![oldText isEqualToString:newText])
      [self resetLabelAttributes];
    [self performActions:_textActions];
  } else {
    NOTREACHED() << "Unexpected label key <" << base::SysNSStringToUTF8(key)
                 << "> observed";
  }
  self.respondingToKVO = NO;
}

@end
