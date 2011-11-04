#!/bin/sh
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

OPTIMIZATION_LEVEL="ADVANCED_OPTIMIZATIONS"
EXTRA_FLAGS=""

while getopts l:d o
do case "$o" in
    l)    EXTRA_FLAGS="$EXTRA_FLAGS -f --define=google.cf.installer.Prompt.\
DEFAULT_IMPLEMENTATION_URL='${OPTARG}cf-xd-install-impl.js'";;
    d)    OPTIMIZATION_LEVEL="SIMPLE_OPTIMIZATIONS"
          EXTRA_FLAGS="$EXTRA_FLAGS -f --formatting=PRETTY_PRINT";;
    [?])  echo >&2 "Usage: $0 [-l //host.com/path/to/scripts/dir/] [-p] [-d]"
          echo >&2 "       -l <URL>   The path under which the generated"
          echo >&2 "                  scripts will be deployed."
          echo >&2 "       -d         Disable obfuscating optimizations."
          exit 1;;
  esac
done

DEPS_DIR=deps
OUT_DIR=out
SRC_DIR=src
CLOSURE_LIBRARY_DIR=$DEPS_DIR/closure_library
CLOSURE_BUILDER=$CLOSURE_LIBRARY_DIR/closure/bin/build/closurebuilder.py
CLOSURE_COMPILER_ZIP=$DEPS_DIR/compiler-latest.zip
CLOSURE_COMPILER_JAR_ZIP_RELATIVE=compiler.jar
CLOSURE_COMPILER_JAR=$DEPS_DIR/$CLOSURE_COMPILER_JAR_ZIP_RELATIVE

mkdir -p $DEPS_DIR &&
mkdir -p $OUT_DIR &&
{ [[ -e $CLOSURE_LIBRARY_DIR ]] || \
  svn checkout http://closure-library.googlecode.com/svn/trunk/ \
    $CLOSURE_LIBRARY_DIR; } && \
{ [[ -e $CLOSURE_COMPILER_JAR ]] ||
  { wget http://closure-compiler.googlecode.com/files/compiler-latest.zip \
      -O $CLOSURE_COMPILER_ZIP && \
    unzip -d $DEPS_DIR $CLOSURE_COMPILER_ZIP \
      $CLOSURE_COMPILER_JAR_ZIP_RELATIVE >/dev/null; }; } &&
$CLOSURE_BUILDER \
  --root=$CLOSURE_LIBRARY_DIR --root=src/common/ \
  --root=src/implementation/ \
  --namespace="google.cf.installer.CrossDomainInstall" \
  --output_mode=compiled --compiler_jar=$CLOSURE_COMPILER_JAR \
  $EXTRA_FLAGS \
  -f "--compilation_level=$OPTIMIZATION_LEVEL" \
  -f "--output_wrapper='(function (scope){ %output% })(window)'" \
  -f "--externs=src/common/cf-interactiondelegate-externs.js" \
  --output_file=out/cf-xd-install-impl.js && \
$CLOSURE_BUILDER \
  --root=src/miniclosure/ --root=src/common/ \
  --root=src/stub/ \
  --namespace="google.cf.installer.CFInstall" \
  --output_mode=compiled --compiler_jar=$CLOSURE_COMPILER_JAR \
  $EXTRA_FLAGS \
  -f "--compilation_level=$OPTIMIZATION_LEVEL" \
  -f "--output_wrapper='(function (scope){ %output% })(window)'" \
  -f "--externs=src/stub/cf-activex-externs.js" \
  -f "--externs=src/common/cf-interactiondelegate-externs.js" \
  --output_file=out/CFInstall.min.js
