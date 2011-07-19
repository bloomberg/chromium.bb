#!/bin/bash

# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script checks the touch_ntp code for common errors and style
# problems using the closure compiler (jscompiler) and closure linter
# (gjslint) - both of which must be on the path.
# See http://code.google.com/closure/compiler/ and
# http://code.google.com/closure/utilities/ for details on these tools.

SOURCES="touchhandler.js slider.js newtab.js grabber.js "
SOURCES+="standalone/standalone_hack.js"

# First run the closure compiler looking for syntactic issues.
# Note that we throw away the output from jscompiler since it's use
# is not yet common in Chromium and for now we want it to be an optional
# tool for helping to find bugs, not something that actually changes
# the embedded JavaScript (making it harder to debug, for example).

# I used to run with '--warning_level VERBOSE' to get full type checking
# but there are enough limitations in the language and compiler that
# it doesn't seem worth the benefit (spent more time trying to apease
# the compiler and reviewers of my code than the compiler saved me).

# Enable support for property get/set syntax as added in ecmascript5.
# Note that this requires a build of JSCompiler that is newer than
# Feb 2011.
CARGS="--language_in=ECMASCRIPT5_STRICT"

CARGS+=" --js_output_file /dev/null"
for S in $SOURCES tools/externs.js; do
    CARGS+=" --js $S"
done

cd `dirname $0`/..

echo jscompiler $CARGS
jscompiler $CARGS || exit 1

# Now run the closure linter looking for style issues.

# GJSLint can't follow the more concice syntax for prototype members and
# complains about missing @this annotations (filed as bug 4073735).  To
# cope for now I just just off all missing-JSDoc warnings.
LARGS="--nojsdoc"

# Verify extra rules like spacing and indentation
LARGS+=" --strict"

# Might as well check the bit of JS we have embedded in HTML too
LARGS+=" --check_html newtab.html"

LARGS+=" $SOURCES"

echo gjslint $LARGS
gjslint $LARGS || exit 1
