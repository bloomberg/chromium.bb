// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/objc_zombie.h"

#include <dlfcn.h>
#include <mach-o/dyld.h>
#include <mach-o/nlist.h>

#import <objc/objc-class.h>

#include "base/logging.h"
#include "base/synchronization/lock.h"
#import "chrome/app/breakpad_mac.h"
#import "chrome/browser/ui/cocoa/objc_method_swizzle.h"

// Deallocated objects are re-classed as |CrZombie|.  No superclass
// because then the class would have to override many/most of the
// inherited methods (|NSObject| is like a category magnet!).
@interface CrZombie {
  Class isa;
}
@end

// Objects with enough space are made into "fat" zombies, which
// directly remember which class they were until reallocated.
@interface CrFatZombie : CrZombie {
 @public
  Class wasa;
}
@end

namespace {

// |object_cxxDestruct()| is an Objective-C runtime function which
// traverses the object's class tree for ".cxxdestruct" methods which
// are run to call C++ destructors as part of |-dealloc|.  The
// function is not public, so must be looked up using nlist.
typedef void DestructFn(id obj);
DestructFn* g_object_cxxDestruct = NULL;

// The original implementation for |-[NSObject dealloc]|.
IMP g_originalDeallocIMP = NULL;

// Classes which freed objects become.  |g_fatZombieSize| is the
// minimum object size which can be made into a fat zombie (which can
// remember which class it was before free, even after falling off the
// treadmill).
Class g_zombieClass = Nil;  // cached [CrZombie class]
Class g_fatZombieClass = Nil;  // cached [CrFatZombie class]
size_t g_fatZombieSize = 0;

// Whether to zombie all freed objects, or only those which return YES
// from |-shouldBecomeCrZombie|.
BOOL g_zombieAllObjects = NO;

// Protects |g_zombieCount|, |g_zombieIndex|, and |g_zombies|.
base::Lock lock_;

// How many zombies to keep before freeing, and the current head of
// the circular buffer.
size_t g_zombieCount = 0;
size_t g_zombieIndex = 0;

typedef struct {
  id object;   // The zombied object.
  Class wasa;  // Value of |object->isa| before we replaced it.
} ZombieRecord;

ZombieRecord* g_zombies = NULL;

// Lookup the private |object_cxxDestruct| function and return a
// pointer to it.  Returns |NULL| on failure.
DestructFn* LookupObjectCxxDestruct() {
#if ARCH_CPU_64_BITS
  // TODO(shess): Port to 64-bit.  I believe using struct nlist_64
  // will suffice.  http://crbug.com/44021 .
  NOTIMPLEMENTED();
  return NULL;
#endif

  struct nlist nl[3];
  bzero(&nl, sizeof(nl));

  nl[0].n_un.n_name = (char*)"_object_cxxDestruct";

  // My ability to calculate the base for offsets is apparently poor.
  // Use |class_addIvar| as a known reference point.
  nl[1].n_un.n_name = (char*)"_class_addIvar";

  if (nlist("/usr/lib/libobjc.dylib", nl) < 0 ||
      nl[0].n_type == N_UNDF || nl[1].n_type == N_UNDF)
    return NULL;

  return (DestructFn*)((char*)&class_addIvar - nl[1].n_value + nl[0].n_value);
}

// Replacement |-dealloc| which turns objects into zombies and places
// them into |g_zombies| to be freed later.
void ZombieDealloc(id self, SEL _cmd) {
  // This code should only be called when it is implementing |-dealloc|.
  DCHECK_EQ(_cmd, @selector(dealloc));

  // Use the original |-dealloc| if the object doesn't wish to be
  // zombied.
  if (!g_zombieAllObjects && ![self shouldBecomeCrZombie]) {
    g_originalDeallocIMP(self, _cmd);
    return;
  }

  // Use the original |-dealloc| if |object_cxxDestruct| was never
  // initialized, because otherwise C++ destructors won't be called.
  // This case should be impossible, but doing it wrong would cause
  // terrible problems.
  DCHECK(g_object_cxxDestruct);
  if (!g_object_cxxDestruct) {
    g_originalDeallocIMP(self, _cmd);
    return;
  }

  Class wasa = object_getClass(self);
  const size_t size = class_getInstanceSize(wasa);

  // Destroy the instance by calling C++ destructors and clearing it
  // to something unlikely to work well if someone references it.
  (*g_object_cxxDestruct)(self);
  memset(self, '!', size);

  // If the instance is big enough, make it into a fat zombie and have
  // it remember the old |isa|.  Otherwise make it a regular zombie.
  // Setting |isa| rather than using |object_setClass()| because that
  // function is implemented with a memory barrier.  The runtime's
  // |_internal_object_dispose()| (in objc-class.m) does this, so it
  // should be safe (messaging free'd objects shouldn't be expected to
  // be thread-safe in the first place).
  if (size >= g_fatZombieSize) {
    self->isa = g_fatZombieClass;
    static_cast<CrFatZombie*>(self)->wasa = wasa;
  } else {
    self->isa = g_zombieClass;
  }

  // The new record to swap into |g_zombies|.  If |g_zombieCount| is
  // zero, then |self| will be freed immediately.
  ZombieRecord zombieToFree = {self, wasa};

  // Don't involve the lock when creating zombies without a treadmill.
  if (g_zombieCount > 0) {
    base::AutoLock pin(lock_);

    // Check the count again in a thread-safe manner.
    if (g_zombieCount > 0) {
      // Put the current object on the treadmill and keep the previous
      // occupant.
      std::swap(zombieToFree, g_zombies[g_zombieIndex]);

      // Bump the index forward.
      g_zombieIndex = (g_zombieIndex + 1) % g_zombieCount;
    }
  }

  // Do the free out here to prevent any chance of deadlock.
  if (zombieToFree.object)
    free(zombieToFree.object);
}

// Attempt to determine the original class of zombie |object|.
Class ZombieWasa(id object) {
  // Fat zombies can hold onto their |wasa| past the point where the
  // object was actually freed.  Note that to arrive here at all,
  // |object|'s memory must still be accessible.
  if (object_getClass(object) == g_fatZombieClass)
    return static_cast<CrFatZombie*>(object)->wasa;

  // For instances which weren't big enough to store |wasa|, check if
  // the object is still on the treadmill.
  base::AutoLock pin(lock_);
  for (size_t i=0; i < g_zombieCount; ++i) {
    if (g_zombies[i].object == object)
      return g_zombies[i].wasa;
  }

  return Nil;
}

// Log a message to a freed object.  |wasa| is the object's original
// class.  |aSelector| is the selector which the calling code was
// attempting to send.  |viaSelector| is the selector of the
// dispatch-related method which is being invoked to send |aSelector|
// (for instance, -respondsToSelector:).
void ZombieObjectCrash(id object, SEL aSelector, SEL viaSelector) {
  Class wasa = ZombieWasa(object);
  const char* wasaName = (wasa ? class_getName(wasa) : "<unknown>");
  NSString* aString =
      [NSString stringWithFormat:@"Zombie <%s: %p> received -%s",
                                 wasaName, object, sel_getName(aSelector)];
  if (viaSelector != NULL) {
    const char* viaName = sel_getName(viaSelector);
    aString = [aString stringByAppendingFormat:@" (via -%s)", viaName];
  }

  // Set a value for breakpad to report, then crash.
  SetCrashKeyValue(@"zombie", aString);
  LOG(ERROR) << [aString UTF8String];

  // This is how about:crash is implemented.  Using instead of
  // |baes::debug::BreakDebugger()| or |LOG(FATAL)| to make the top of
  // stack more immediately obvious in crash dumps.
  int* zero = NULL;
  *zero = 0;
}

// Initialize our globals, returning YES on success.
BOOL ZombieInit() {
  static BOOL initialized = NO;
  if (initialized)
    return YES;

  Class rootClass = [NSObject class];

  g_object_cxxDestruct = LookupObjectCxxDestruct();
  g_originalDeallocIMP =
      class_getMethodImplementation(rootClass, @selector(dealloc));
  // objc_getClass() so CrZombie doesn't need +class.
  g_zombieClass = objc_getClass("CrZombie");
  g_fatZombieClass = objc_getClass("CrFatZombie");
  g_fatZombieSize = class_getInstanceSize(g_fatZombieClass);

  if (!g_object_cxxDestruct || !g_originalDeallocIMP ||
      !g_zombieClass || !g_fatZombieClass)
    return NO;

  initialized = YES;
  return YES;
}

}  // namespace

@implementation CrZombie

// The Objective-C runtime needs to be able to call this successfully.
+ (void)initialize {
}

// Any method not explicitly defined will end up here, forcing a
// crash.
- (id)forwardingTargetForSelector:(SEL)aSelector {
  ZombieObjectCrash(self, aSelector, NULL);
  return nil;
}

// Override a few methods often used for dynamic dispatch to log the
// message the caller is attempting to send, rather than the utility
// method being used to send it.
- (BOOL)respondsToSelector:(SEL)aSelector {
  ZombieObjectCrash(self, aSelector, _cmd);
  return NO;
}

- (id)performSelector:(SEL)aSelector {
  ZombieObjectCrash(self, aSelector, _cmd);
  return nil;
}

- (id)performSelector:(SEL)aSelector withObject:(id)anObject {
  ZombieObjectCrash(self, aSelector, _cmd);
  return nil;
}

- (id)performSelector:(SEL)aSelector
           withObject:(id)anObject
           withObject:(id)anotherObject {
  ZombieObjectCrash(self, aSelector, _cmd);
  return nil;
}

- (void)performSelector:(SEL)aSelector
             withObject:(id)anArgument
             afterDelay:(NSTimeInterval)delay {
  ZombieObjectCrash(self, aSelector, _cmd);
}

@end

@implementation CrFatZombie

// This implementation intentionally left empty.

@end

@implementation NSObject (CrZombie)

- (BOOL)shouldBecomeCrZombie {
  return NO;
}

@end

namespace ObjcEvilDoers {

BOOL ZombieEnable(BOOL zombieAllObjects,
                  size_t zombieCount) {
  // Only allow enable/disable on the main thread, just to keep things
  // simple.
  CHECK([NSThread isMainThread]);

  if (!ZombieInit())
    return NO;

  g_zombieAllObjects = zombieAllObjects;

  // Replace the implementation of -[NSObject dealloc].
  Method m = class_getInstanceMethod([NSObject class], @selector(dealloc));
  if (!m)
    return NO;

  const IMP prevDeallocIMP = method_setImplementation(m, (IMP)ZombieDealloc);
  DCHECK(prevDeallocIMP == g_originalDeallocIMP ||
         prevDeallocIMP == (IMP)ZombieDealloc);

  // Grab the current set of zombies.  This is thread-safe because
  // only the main thread can change these.
  const size_t oldCount = g_zombieCount;
  ZombieRecord* oldZombies = g_zombies;

  {
    base::AutoLock pin(lock_);

    // Save the old index in case zombies need to be transferred.
    size_t oldIndex = g_zombieIndex;

    // Create the new zombie treadmill, disabling zombies in case of
    // failure.
    g_zombieIndex = 0;
    g_zombieCount = zombieCount;
    g_zombies = NULL;
    if (g_zombieCount) {
      g_zombies =
          static_cast<ZombieRecord*>(calloc(g_zombieCount, sizeof(*g_zombies)));
      if (!g_zombies) {
        NOTREACHED();
        g_zombies = oldZombies;
        g_zombieCount = oldCount;
        g_zombieIndex = oldIndex;
        ZombieDisable();
        return NO;
      }
    }

    // If the count is changing, allow some of the zombies to continue
    // shambling forward.
    const size_t sharedCount = std::min(oldCount, zombieCount);
    if (sharedCount) {
      // Get index of the first shared zombie.
      oldIndex = (oldIndex + oldCount - sharedCount) % oldCount;

      for (; g_zombieIndex < sharedCount; ++ g_zombieIndex) {
        DCHECK_LT(g_zombieIndex, g_zombieCount);
        DCHECK_LT(oldIndex, oldCount);
        std::swap(g_zombies[g_zombieIndex], oldZombies[oldIndex]);
        oldIndex = (oldIndex + 1) % oldCount;
      }
      g_zombieIndex %= g_zombieCount;
    }
  }

  // Free the old treadmill and any remaining zombies.
  if (oldZombies) {
    for (size_t i = 0; i < oldCount; ++i) {
      if (oldZombies[i].object)
        free(oldZombies[i].object);
    }
    free(oldZombies);
  }

  return YES;
}

void ZombieDisable() {
  // Only allow enable/disable on the main thread, just to keep things
  // simple.
  CHECK([NSThread isMainThread]);

  // |ZombieInit()| was never called.
  if (!g_originalDeallocIMP)
    return;

  // Put back the original implementation of -[NSObject dealloc].
  Method m = class_getInstanceMethod([NSObject class], @selector(dealloc));
  CHECK(m);
  method_setImplementation(m, g_originalDeallocIMP);

  // Can safely grab this because it only happens on the main thread.
  const size_t oldCount = g_zombieCount;
  ZombieRecord* oldZombies = g_zombies;

  {
    base::AutoLock pin(lock_);  // In case any |-dealloc| are in-progress.
    g_zombieCount = 0;
    g_zombies = NULL;
  }

  // Free any remaining zombies.
  if (oldZombies) {
    for (size_t i = 0; i < oldCount; ++i) {
      if (oldZombies[i].object)
        free(oldZombies[i].object);
    }
    free(oldZombies);
  }
}

}  // namespace ObjcEvilDoers
