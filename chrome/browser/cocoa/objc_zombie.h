// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_NSOBJECT_ZOMBIE_H_
#define CHROME_BROWSER_COCOA_NSOBJECT_ZOMBIE_H_

#import <Foundation/Foundation.h>

// You should think twice every single time you use anything from this
// namespace.
namespace ObjcEvilDoers {

// Enable zombies.  Returns NO if it fails to enable.
//
// When |zombieAllObjects| is YES, all objects inheriting from
// NSObject become zombies on -dealloc.  If NO, -shouldBecomeCrZombie
// is queried to determine whether to make the object a zombie.
//
// |zombieCount| controls how many zombies to store before freeing the
// oldest.  Set to 0 to free objects immediately after making them
// zombies.
BOOL ZombieEnable(BOOL zombieAllObjects, size_t zombieCount);

// Disable zombies.
void ZombieDisable();

}  // namespace ObjcEvilDoers

@interface NSObject (CrZombie)
- (BOOL)shouldBecomeCrZombie;
@end

#endif  // CHROME_BROWSER_COCOA_NSOBJECT_ZOMBIE_H_
