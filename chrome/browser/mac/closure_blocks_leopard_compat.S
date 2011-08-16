# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Definitions of symbols that may be needed at runtime but aren't necessarily
# present in the SDK chosen for compilation.
#
# This file provides symbols for _NSConcreteGlobalBlock and
# _NSConcreteStackBlock, normally present in libSystem.dylib and provided by
# by libclosure-38/data.c in Mac OS X 10.6 and later. It also provides symbols
# for various block runtime functions provided by libclosure-38/runtime.c.
# When using the 10.5 SDK, the symbols are not present. This file's definition
# can be used with extreme care in an application that needs to use the 10.5
# SDK in conjunction with blocks.
#
# This file cooperates with the build system to produce a dynamic library
# that, when linked against, causes dependents to look in libSystem for the
# symbols provided here. It also cooperates with a header that causes
# dependents to treat the symbols provided here as weak imports, critical for
# the resultant output to be loadable on 10.5.

# To simplify things, this file assumes it's being built with the 10.5 SDK,
# a deployment target of 10.5, and is producing 32-bit x86 code. Other
# combinations are possible, but not interesting for the time being. See
# <sys/cdefs.h> for interesting ways that names might be mangled in other
# configurations.

#include <AvailabilityMacros.h>

#if MAC_OS_X_VERSION_MIN_REQUIRED != MAC_OS_X_VERSION_10_5 || \
    MAC_OS_X_VERSION_MAX_ALLOWED != MAC_OS_X_VERSION_10_5 || \
    !defined(__i386__)
#error This file only supports 32-bit x86 code with both SDK and DT set to 10.5
#endif

#define DEFINE_GLOBAL_SYMBOL(name) \
    .globl name ## ;\
    name ## :

.text

# Mac OS X 10.6.8 libclosure-38/runtime.c

DEFINE_GLOBAL_SYMBOL(__Block_copy)
DEFINE_GLOBAL_SYMBOL(__Block_release)
DEFINE_GLOBAL_SYMBOL(__Block_object_assign)
DEFINE_GLOBAL_SYMBOL(__Block_object_dispose)

.section __DATA,__data

# Mac OS X 10.6.8 libclosure-38/data.c

DEFINE_GLOBAL_SYMBOL(__NSConcreteGlobalBlock)
DEFINE_GLOBAL_SYMBOL(__NSConcreteStackBlock)

# When this file is in use, the linker is expected to link things against both
# this file and the real copy of libSystem present in the SDK. When doing so,
# the linker is smart enough to produce only one LC_LOAD_DYLIB load command.
# However, it's not smart enough to notice that while this file's dylib only
# provides weak-imported symbols, the real libSystem's dylib does not.
# Consequently, it may produce an LC_LOAD_WEAK_DYLIB load command for
# libSystem instead of an ordinary LC_LOAD_DYLIB command. LC_LOAD_WEAK_DYLIB
# declares that any symbol offered by the library, and in fact the entire
# library, is permitted to be missing at runtime. This is entirely
# inappropriate for libSystem. To counteract this problem, this file also
# defines some other symbols that libSystem provides. Dependents of this
# library are not expected to treat these other symbols as weak imports. In
# order for any dependent that links against this library to load it with an
# LC_LOAD_DYLIB command instead of an LC_LOAD_WEAK_DYLIB command, this library
# must satisfy at least one unresolved non-weak-import symbol required by the
# dependent.

.text

# |exit| is a good one: because it's referenced by crt1.o, ordinary executables
# are guaranteed to need this symbol. Unfortunately, there's no such symbol in
# dylib1.o that libSystem is expected to provide, so a few other common libc
# symbols are thrown into the mix.
DEFINE_GLOBAL_SYMBOL(_exit)

# Include |close| because well-written programs that use the standard library
# are likely to refer to it. Include |open| for good measure because it goes
# pretty well with this. Include the stdio abstractions for these functions
# as well.
DEFINE_GLOBAL_SYMBOL(_close$UNIX2003)
DEFINE_GLOBAL_SYMBOL(_open$UNIX2003)
DEFINE_GLOBAL_SYMBOL(_fclose)
DEFINE_GLOBAL_SYMBOL(_fopen)
DEFINE_GLOBAL_SYMBOL(_fdopen)
DEFINE_GLOBAL_SYMBOL(_freopen$UNIX2003)

# Commonly-used allocation functions.
DEFINE_GLOBAL_SYMBOL(_malloc)
DEFINE_GLOBAL_SYMBOL(_calloc)
DEFINE_GLOBAL_SYMBOL(_realloc)
DEFINE_GLOBAL_SYMBOL(_reallocf)
DEFINE_GLOBAL_SYMBOL(_valloc)
DEFINE_GLOBAL_SYMBOL(_free)

# Include |printf|, |fprintf|, |sprintf|, |snprintf|, and |puts|, because
# small test programs are likely to refer to one of these. puts is rarely
# invoked directly, but the compiler may optimize simple printf calls into
# puts calls.
DEFINE_GLOBAL_SYMBOL(_printf)
DEFINE_GLOBAL_SYMBOL(_fprintf)
DEFINE_GLOBAL_SYMBOL(_sprintf)
DEFINE_GLOBAL_SYMBOL(_snprintf)
DEFINE_GLOBAL_SYMBOL(_puts)

# Some <string.h> functions that are commonly used.
DEFINE_GLOBAL_SYMBOL(_memcmp)
DEFINE_GLOBAL_SYMBOL(_memcpy)
DEFINE_GLOBAL_SYMBOL(_memmove)
DEFINE_GLOBAL_SYMBOL(_memset)
DEFINE_GLOBAL_SYMBOL(_strcasecmp)
DEFINE_GLOBAL_SYMBOL(_strcat)
DEFINE_GLOBAL_SYMBOL(_strchr)
DEFINE_GLOBAL_SYMBOL(_strcmp)
DEFINE_GLOBAL_SYMBOL(_strcpy)
DEFINE_GLOBAL_SYMBOL(_strdup)
DEFINE_GLOBAL_SYMBOL(_strlcat)
DEFINE_GLOBAL_SYMBOL(_strlcpy)
DEFINE_GLOBAL_SYMBOL(_strlen)
DEFINE_GLOBAL_SYMBOL(_strncasecmp)
DEFINE_GLOBAL_SYMBOL(_strncat)
DEFINE_GLOBAL_SYMBOL(_strncmp)
DEFINE_GLOBAL_SYMBOL(_strncpy)
DEFINE_GLOBAL_SYMBOL(_strnstr)
DEFINE_GLOBAL_SYMBOL(_strstr)

# Some data-section symbols that might be referenced.

.section __DATA,__data

DEFINE_GLOBAL_SYMBOL(___stdinp)
DEFINE_GLOBAL_SYMBOL(___stdoutp)
DEFINE_GLOBAL_SYMBOL(___stderrp)

#undef DEFINE_GLOBAL_SYMBOL
