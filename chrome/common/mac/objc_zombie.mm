// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/common/mac/objc_zombie.h"

#include <dlfcn.h>
#include <execinfo.h>
#include <mach-o/dyld.h>
#include <mach-o/nlist.h>

#import <objc/objc-class.h>

#include <algorithm>
#include <iostream>

#include "base/debug/stack_trace.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/mac/crash_logging.h"
#include "base/mac/mac_util.h"
#include "base/metrics/histogram.h"
#include "base/synchronization/lock.h"
#import "chrome/common/mac/objc_method_swizzle.h"

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

// The depth of backtrace to store with zombies.  This directly influences
// the amount of memory required to track zombies, so should be kept as
// small as is useful.  Unfortunately, too small and it won't poke through
// deep autorelease and event loop stacks.
// NOTE(shess): Breakpad currently restricts values to 255 bytes.  The
// trace is hex-encoded with "0x" prefix and " " separators, meaning
// the maximum number of 32-bit items which can be encoded is 23.
const size_t kBacktraceDepth = 20;

// Function which destroys the contents of an object without freeing
// the object.  On 10.5 this is |object_cxxDestruct()|, which
// traverses the class hierarchy to run the C++ destructors.  On 10.6
// this is |objc_destructInstance()| which calls
// |object_cxxDestruct()| and removes associative references.
// |objc_destructInstance()| returns |void*| but pretending it has no
// return value makes the code simpler.
typedef void DestructFn(id obj);
DestructFn* g_objectDestruct = NULL;

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
base::LazyInstance<base::Lock>::Leaky g_lock = LAZY_INSTANCE_INITIALIZER;

// How many zombies to keep before freeing, and the current head of
// the circular buffer.
size_t g_zombieCount = 0;
size_t g_zombieIndex = 0;

typedef struct {
  id object;   // The zombied object.
  Class wasa;  // Value of |object->isa| before we replaced it.
  void* trace[kBacktraceDepth];  // Backtrace at point of deallocation.
  size_t traceDepth;             // Actual depth of trace[].
} ZombieRecord;

ZombieRecord* g_zombies = NULL;

const char* LookupObjcRuntimePath() {
  const void* addr = reinterpret_cast<void*>(&object_getClass);
  Dl_info info;

  // |dladdr()| doesn't document how long |info| will stay valid...
  if (dladdr(addr, &info))
    return info.dli_fname;

  return NULL;
}

// Lookup |objc_destructInstance()| dynamically because it isn't
// available on 10.5, but we link with the 10.5 SDK.
DestructFn* LookupObjectDestruct_10_6() {
  const char* objc_runtime_path = LookupObjcRuntimePath();
  if (!objc_runtime_path)
    return NULL;

  void* handle = dlopen(objc_runtime_path, RTLD_LAZY | RTLD_LOCAL);
  if (!handle)
    return NULL;

  void* fn = dlsym(handle, "objc_destructInstance");

  // |fn| would normally be expected to become invalid after this
  // |dlclose()|, but since the |dlopen()| was on a library
  // containing an already-mapped symbol, it will remain valid.
  dlclose(handle);
  return reinterpret_cast<DestructFn*>(fn);
}

// Under 10.5 |object_cxxDestruct()| is used but unfortunately it is
// |__private_extern__| in the runtime, meaning |dlsym()| cannot reach it.
DestructFn* LookupObjectDestruct_10_5() {
  // |nlist()| is only present for 32-bit.
#if ARCH_CPU_32_BITS
  const char* objc_runtime_path = LookupObjcRuntimePath();
  if (!objc_runtime_path)
    return NULL;

  struct nlist nl[3];
  bzero(&nl, sizeof(nl));

  nl[0].n_un.n_name = const_cast<char*>("_object_cxxDestruct");

  // My ability to calculate the base for offsets is apparently poor.
  // Use |class_addIvar| as a known reference point.
  nl[1].n_un.n_name = const_cast<char*>("_class_addIvar");

  if (nlist(objc_runtime_path, nl) < 0 ||
      nl[0].n_type == N_UNDF || nl[1].n_type == N_UNDF)
    return NULL;

  // Back out offset to |class_addIvar()| to get the baseline, then
  // add back offset to |object_cxxDestruct()|.
  uintptr_t reference_addr = reinterpret_cast<uintptr_t>(&class_addIvar);
  reference_addr -= nl[1].n_value;
  reference_addr += nl[0].n_value;

  return reinterpret_cast<DestructFn*>(reference_addr);
#endif

  return NULL;
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

  // Use the original |-dealloc| if |g_objectDestruct| was never
  // initialized, because otherwise C++ destructors won't be called.
  // This case should be impossible, but doing it wrong would cause
  // terrible problems.
  DCHECK(g_objectDestruct);
  if (!g_objectDestruct) {
    g_originalDeallocIMP(self, _cmd);
    return;
  }

  Class wasa = object_getClass(self);
  const size_t size = class_getInstanceSize(wasa);

  // Destroy the instance by calling C++ destructors and clearing it
  // to something unlikely to work well if someone references it.
  // NOTE(shess): |object_dispose()| will call this again when the
  // zombie falls off the treadmill!  But by then |isa| will be a
  // class without C++ destructors or associative references, so it
  // won't hurt anything.
  (*g_objectDestruct)(self);
  memset(self, '!', size);

  // If the instance is big enough, make it into a fat zombie and have
  // it remember the old |isa|.  Otherwise make it a regular zombie.
  // Setting |isa| rather than using |object_setClass()| because that
  // function is implemented with a memory barrier.  The runtime's
  // |_internal_object_dispose()| (in objc-class.m) does this, so it
  // should be safe (messaging free'd objects shouldn't be expected to
  // be thread-safe in the first place).
#pragma clang diagnostic push  // clang warns about direct access to isa.
#pragma clang diagnostic ignored "-Wdeprecated-objc-isa-usage"
  if (size >= g_fatZombieSize) {
    self->isa = g_fatZombieClass;
    static_cast<CrFatZombie*>(self)->wasa = wasa;
  } else {
    self->isa = g_zombieClass;
  }
#pragma clang diagnostic pop

  // The new record to swap into |g_zombies|.  If |g_zombieCount| is
  // zero, then |self| will be freed immediately.
  ZombieRecord zombieToFree = {self, wasa};
  zombieToFree.traceDepth =
      std::max(backtrace(zombieToFree.trace, kBacktraceDepth), 0);

  // Don't involve the lock when creating zombies without a treadmill.
  if (g_zombieCount > 0) {
    base::AutoLock pin(g_lock.Get());

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
    object_dispose(zombieToFree.object);
}

// Search the treadmill for |object| and fill in |*record| if found.
// Returns YES if found.
BOOL GetZombieRecord(id object, ZombieRecord* record) {
  // Holding the lock is reasonable because this should be fast, and
  // the process is going to crash presently anyhow.
  base::AutoLock pin(g_lock.Get());
  for (size_t i = 0; i < g_zombieCount; ++i) {
    if (g_zombies[i].object == object) {
      *record = g_zombies[i];
      return YES;
    }
  }
  return NO;
}

// Dump the symbols.  This is pulled out into a function to make it
// easy to use DCHECK to dump only in debug builds.
BOOL DumpDeallocTrace(const void* const* array, int size) {
  // |cerr| because that's where PrintBacktrace() sends output.
  std::cerr << "Backtrace from -dealloc:\n";
  base::debug::StackTrace(array, size).PrintBacktrace();

  return YES;
}

// Log a message to a freed object.  |wasa| is the object's original
// class.  |aSelector| is the selector which the calling code was
// attempting to send.  |viaSelector| is the selector of the
// dispatch-related method which is being invoked to send |aSelector|
// (for instance, -respondsToSelector:).
void ZombieObjectCrash(id object, SEL aSelector, SEL viaSelector) {
  ZombieRecord record;
  BOOL found = GetZombieRecord(object, &record);

  // The object's class can be in the zombie record, but if that is
  // not available it can also be in the object itself (in most cases).
  Class wasa = Nil;
  if (found) {
    wasa = record.wasa;
  } else if (object_getClass(object) == g_fatZombieClass) {
    wasa = static_cast<CrFatZombie*>(object)->wasa;
  }
  const char* wasaName = (wasa ? class_getName(wasa) : "<unknown>");

  NSString* aString =
      [NSString stringWithFormat:@"Zombie <%s: %p> received -%s",
                                 wasaName, object, sel_getName(aSelector)];
  if (viaSelector != NULL) {
    const char* viaName = sel_getName(viaSelector);
    aString = [aString stringByAppendingFormat:@" (via -%s)", viaName];
  }

  // Set a value for breakpad to report.
  base::mac::SetCrashKeyValue(@"zombie", aString);

  // Encode trace into a breakpad key.
  if (found) {
    base::mac::SetCrashKeyFromAddresses(
        @"zombie_dealloc_bt", record.trace, record.traceDepth);
  }

  // Log -dealloc backtrace in debug builds then crash with a useful
  // stack trace.
  if (found && record.traceDepth) {
    DCHECK(DumpDeallocTrace(record.trace, record.traceDepth));
  } else {
    DLOG(INFO) << "Unable to generate backtrace from -dealloc.";
  }
  DLOG(FATAL) << [aString UTF8String];

  // This is how about:crash is implemented.  Using instead of
  // |base::debug::BreakDebugger()| or |LOG(FATAL)| to make the top of
  // stack more immediately obvious in crash dumps.
  int* zero = NULL;
  *zero = 0;
}

// For monitoring failures in |ZombieInit()|.
enum ZombieFailure {
  FAILED_10_5,
  FAILED_10_6,

  // Add new versions before here.
  FAILED_MAX,
};

void RecordZombieFailure(ZombieFailure failure) {
  UMA_HISTOGRAM_ENUMERATION("OSX.ZombieInitFailure", failure, FAILED_MAX);
}

// Initialize our globals, returning YES on success.
BOOL ZombieInit() {
  static BOOL initialized = NO;
  if (initialized)
    return YES;

  // Whitelist releases that are compatible with objc zombies.
  if (base::mac::IsOSLeopard()) {
    g_objectDestruct = LookupObjectDestruct_10_5();
    if (!g_objectDestruct) {
      RecordZombieFailure(FAILED_10_5);
      return NO;
    }
  } else if (base::mac::IsOSSnowLeopard()) {
    g_objectDestruct = LookupObjectDestruct_10_6();
    if (!g_objectDestruct) {
      RecordZombieFailure(FAILED_10_6);
      return NO;
    }
  } else if (base::mac::IsOSLionOrLater()) {
    // Assume the future looks like the present.
    g_objectDestruct = LookupObjectDestruct_10_6();

    // Put all future failures into the MAX bin.  New OS releases come
    // out infrequently enough that this should always correspond to
    // "Next release", and once the next release happens that bin will
    // get an official name.
    if (!g_objectDestruct) {
      RecordZombieFailure(FAILED_MAX);
      return NO;
    }
  } else {
    return NO;
  }

  Class rootClass = [NSObject class];
  g_originalDeallocIMP =
      class_getMethodImplementation(rootClass, @selector(dealloc));
  // objc_getClass() so CrZombie doesn't need +class.
  g_zombieClass = objc_getClass("CrZombie");
  g_fatZombieClass = objc_getClass("CrFatZombie");
  g_fatZombieSize = class_getInstanceSize(g_fatZombieClass);

  if (!g_objectDestruct || !g_originalDeallocIMP ||
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

bool ZombieEnable(bool zombieAllObjects,
                  size_t zombieCount) {
  // Only allow enable/disable on the main thread, just to keep things
  // simple.
  DCHECK([NSThread isMainThread]);

  if (!ZombieInit())
    return false;

  g_zombieAllObjects = zombieAllObjects;

  // Replace the implementation of -[NSObject dealloc].
  Method m = class_getInstanceMethod([NSObject class], @selector(dealloc));
  if (!m)
    return false;

  const IMP prevDeallocIMP = method_setImplementation(m, (IMP)ZombieDealloc);
  DCHECK(prevDeallocIMP == g_originalDeallocIMP ||
         prevDeallocIMP == (IMP)ZombieDealloc);

  // Grab the current set of zombies.  This is thread-safe because
  // only the main thread can change these.
  const size_t oldCount = g_zombieCount;
  ZombieRecord* oldZombies = g_zombies;

  {
    base::AutoLock pin(g_lock.Get());

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
        return false;
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
        object_dispose(oldZombies[i].object);
    }
    free(oldZombies);
  }

  return true;
}

void ZombieDisable() {
  // Only allow enable/disable on the main thread, just to keep things
  // simple.
  DCHECK([NSThread isMainThread]);

  // |ZombieInit()| was never called.
  if (!g_originalDeallocIMP)
    return;

  // Put back the original implementation of -[NSObject dealloc].
  Method m = class_getInstanceMethod([NSObject class], @selector(dealloc));
  DCHECK(m);
  method_setImplementation(m, g_originalDeallocIMP);

  // Can safely grab this because it only happens on the main thread.
  const size_t oldCount = g_zombieCount;
  ZombieRecord* oldZombies = g_zombies;

  {
    base::AutoLock pin(g_lock.Get());  // In case any -dealloc are in progress.
    g_zombieCount = 0;
    g_zombies = NULL;
  }

  // Free any remaining zombies.
  if (oldZombies) {
    for (size_t i = 0; i < oldCount; ++i) {
      if (oldZombies[i].object)
        object_dispose(oldZombies[i].object);
    }
    free(oldZombies);
  }
}

}  // namespace ObjcEvilDoers
