#!/bin/bash
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -o xtrace
set -o nounset
set -o errexit

######################################################################
# SCRIPT CONFIG
######################################################################

CLOBBER=${CLOBBER:-yes}
SCONS_TRUSTED="./scons --mode=opt-host -j8"
SCONS_NACL="./scons --mode=opt-host,nacl -j8"
SPEC_HARNESS=${SPEC_HARNESS:-${HOME}/cpu2000-redhat64-ia32}/

TRYBOT_TESTS="176.gcc 179.art 181.mcf 197.parser 252.eon 254.gap"
TRYBOT_TRANSLATOR_TESTS="176.gcc"

SPEC_BASE="tests/spec2k"

RETCODE=0

######################################################################
# SCRIPT ACTION
######################################################################

clobber() {
  if [ "${CLOBBER}" == "yes" ] ; then
    echo "@@@BUILD_STEP clobber@@@"
    rm -rf scons-out toolchain

    echo "@@@BUILD_STEP gclient_runhooks@@@"
    gclient runhooks --force
  fi
}

# Make up for the toolchain tarballs not quite being a full SDK
build-prerequsites() {
  echo "@@@BUILD_STEP build prerequisites [$*] @@@"
  pushd ${SPEC_BASE}
  ./run_all.sh BuildPrerequisites "$@"
  popd
}

# Build and run a small sample of the tests for trybots.
# These do not do any timing or size measurements.
build-and-run-some() {
  local setups="$1"
  local tests="$2"

  pushd ${SPEC_BASE}
  for setup in ${setups}; do
    echo "@@@BUILD_STEP spec2k build [${setup}] [train-some]@@@"
    ./run_all.sh CleanBenchmarks
    ./run_all.sh PopulateFromSpecHarness "${SPEC_HARNESS}"
    MAKEOPTS=-j8 \
      ./run_all.sh BuildBenchmarks 0 ${setup} ${tests}

    echo "@@@BUILD_STEP spec2k run [${setup}] [train-some]@@@"
    ./run_all.sh RunBenchmarks ${setup} train ${tests} || \
      { RETCODE=$? && echo "@@@STEP_FAILURE@@@"; }
  done
  popd
}

# Build and run all of the tests and do timing + size measurements.
build-and-run-all() {
  local setups="$1"
  local run_repetitions=$2
  local compile_repetitions=$2

  pushd ${SPEC_BASE}
  for setup in ${setups}; do
    echo "@@@BUILD_STEP spec2k build [${setup}] [train]@@@"
    ./run_all.sh CleanBenchmarks
    ./run_all.sh PopulateFromSpecHarness "${SPEC_HARNESS}"
    MAKEOPTS=-j8 \
    SPEC_COMPILE_REPETITIONS=${compile_repetitions} \
      ./run_all.sh BuildBenchmarks 1 ${setup} train

    echo @@@BUILD_STEP spec2k run [${setup}] [train]@@@
    SPEC_RUN_REPETITIONS=${run_repetitions} \
      ./run_all.sh RunTimedBenchmarks ${setup} train || \
      { RETCODE=$? && echo "@@@STEP_FAILURE@@@"; }
  done
  popd
}


######################################################################
# NOTE: trybots only runs a subset of the the spec2k tests
# TODO: elminate this long running bot in favor per arch sharded bots
pnacl-trybot-arm() {
  clobber
  build-prerequsites "arm" "bitcode"
  build-and-run-some SetupPnaclArmOpt "${TRYBOT_TESTS}"
}

pnacl-trybot-x8632() {
  clobber
  build-prerequsites "x86-32" "bitcode"
  build-and-run-some SetupPnaclX8632Opt "${TRYBOT_TESTS}"
  build-and-run-some SetupPnaclTranslatorX8632Opt "${TRYBOT_TRANSLATOR_TESTS}"
}

pnacl-trybot-x8664() {
  clobber
  build-prerequsites "x86-64" "bitcode"
  build-and-run-some SetupPnaclX8664Opt "${TRYBOT_TESTS}"
  build-and-run-some SetupPnaclTranslatorX8664Opt "${TRYBOT_TRANSLATOR_TESTS}"
}

pnacl-arm() {
  clobber
  build-prerequsites "arm" "bitcode"
  # arm takes a long time and we do not have sandboxed tests working
  build-and-run-all "SetupPnaclArmOpt" 1 1
}

pnacl-x8664() {
  clobber
  build-prerequsites "x86-64" "bitcode"
  build-and-run-all "SetupPnaclX8664 \
                     SetupPnaclX8664Opt \
                     SetupPnaclTranslatorX8664 \
                     SetupPnaclTranslatorX8664Opt" 3 3
}

pnacl-x8632() {
  clobber
  build-prerequsites "x86-32" "bitcode"
  build-and-run-all "SetupPnaclX8632 \
                     SetupPnaclX8632Opt \
                     SetupPnaclTranslatorX8632 \
                     SetupPnaclTranslatorX8632Opt" 3 3
}

nacl-x8632() {
  clobber
  build-prerequsites "x86-32" ""
  build-and-run-all "SetupNaclX8632 \
                     SetupNaclX8632Opt" 3 3
}

nacl-x8664() {
  clobber
  build-prerequsites "x86-64" ""
  build-and-run-all "SetupNaclX8664 \
                     SetupNaclX8664Opt" 3 3

}


######################################################################
# Script assumed to be run in native_client/
if [[ $(pwd) != */native_client ]]; then
  echo "ERROR: must be run in native_client!"
  exit 1
fi


if [[ $# -eq 0 ]] ; then
  echo "you must specify a mode on the commandline:"
  exit 1
fi

if [ "$(type -t $1)" != "function" ]; then
  Usage
  echo "ERROR: unknown mode '$1'." >&2
  exit 1
fi

"$@"

if [[ ${RETCODE} != 0 ]]; then
  echo "@@@BUILD_STEP summary@@@"
  echo There were failed stages.
  exit ${RETCODE}
fi
