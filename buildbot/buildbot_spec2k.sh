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
UTMAN=tools/llvm/utman.sh

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

basic-setup-nacl() {
  local platforms=$1
  build-sel_ldr "${platforms}"
  build-libs "${platforms}"
  build-irt "${platforms}"
}

basic-setup-pnacl() {
  local platforms=$1
  build-sel_ldr "${platforms}"
  build-sel_universal "${platforms}"
  build-irt "${platforms}"
  ${UTMAN} sdk
}

build-sel_ldr() {
  local platforms=$1
  for platform in ${platforms} ; do
    echo "@@@BUILD_STEP scons sel_ldr [${platform}]@@@"
    ${SCONS_TRUSTED} platform=${platform} sel_ldr
  done
}

build-sel_universal() {
  local platforms=$1
  for platform in ${platforms} ; do
    echo "@@@BUILD_STEP scons sel_universal [${platform}]@@@"
    ${SCONS_TRUSTED} platform=${platform} sel_universal
  done
}

build-irt() {
  local platforms=$1
  shift 1
  for platform in ${platforms} ; do
    echo "@@@BUILD_STEP scons irt_core [${platform}] $* @@@"
    ${SCONS_NACL} platform=${platform} irt_core "$@"
  done
}

build-libs() {
  local platforms=$1
  shift 1
  for platform in ${platforms} ; do
    echo "@@@BUILD_STEP scons build_lib [${platform}] $* @@@"
    ${SCONS_NACL} platform=${platform} build_lib "$@"
  done
}

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

build-and-run-all() {
  local setups="$1"

  pushd ${SPEC_BASE}
  for setup in ${setups}; do
    echo "@@@BUILD_STEP spec2k build [${setup}] [train]@@@"
    ./run_all.sh CleanBenchmarks
    ./run_all.sh PopulateFromSpecHarness "${SPEC_HARNESS}"
    MAKEOPTS=-j8 \
      ./run_all.sh BuildBenchmarks 1 ${setup} train

    echo @@@BUILD_STEP spec2k run [${setup}] [train]@@@
    # NOTE: we intentionally do not parallelize the build because
    # we are measuring build times
    ./run_all.sh RunTimedBenchmarks ${setup} train || \
      { RETCODE=$? && echo "@@@STEP_FAILURE@@@"; }
  done
  popd
}


######################################################################
# NOTE: trybots only runs a subset of the the spec2k tests
# TODO: elminate this long running bot in favor per arch sharded bots
pnacl-trybot() {
  clobber
  basic-setup-pnacl "arm x86-64 x86-32"
  build-and-run-some SetupPnaclArmOpt "${TRYBOT_TESTS}"
  build-and-run-some SetupPnaclX8632Opt "${TRYBOT_TESTS}"
  build-and-run-some SetupPnaclX8664Opt "${TRYBOT_TESTS}"
  build-and-run-some SetupPnaclTranslatorX8632Opt "${TRYBOT_TRANSLATOR_TESTS}"
  build-and-run-some SetupPnaclTranslatorX8664Opt "${TRYBOT_TRANSLATOR_TESTS}"
}

pnacl-trybot-arm() {
  clobber
  basic-setup-pnacl "arm"
  build-and-run-some SetupPnaclArmOpt "${TRYBOT_TESTS}"
}

pnacl-trybot-x8632() {
  clobber
  basic-setup-pnacl "x86-32"
  build-and-run-some SetupPnaclX8632Opt "${TRYBOT_TESTS}"
  build-and-run-some SetupPnaclTranslatorX8632Opt "${TRYBOT_TRANSLATOR_TESTS}"
}

pnacl-trybot-x8664() {
  clobber
  basic-setup-pnacl "x86-64"
  build-and-run-some SetupPnaclX8664Opt "${TRYBOT_TESTS}"
  build-and-run-some SetupPnaclTranslatorX8664Opt "${TRYBOT_TRANSLATOR_TESTS}"
}

pnacl-arm() {
  clobber
  basic-setup-pnacl "arm"
  # arm takes a long time and we do not have sandboxed tests working
  build-and-run-all "SetupPnaclArmOpt"
}

pnacl-x8664() {
  clobber
  basic-setup-pnacl "x86-64"
  build-and-run-all "SetupPnaclX8664 \
                     SetupPnaclX8664Opt \
                     SetupPnaclTranslatorX8664 \
                     SetupPnaclTranslatorX8664Opt"
}

pnacl-x8632() {
  clobber
  basic-setup-pnacl "x86-32"
  build-and-run-all "SetupPnaclX8632 \
                     SetupPnaclX8632Opt \
                     SetupPnaclTranslatorX8632 \
                     SetupPnaclTranslatorX8632Opt"
}

nacl-x8632() {
  clobber
  basic-setup-nacl "x86-32"
  build-and-run-all "SetupNaclX8632 \
                     SetupNaclX8632Opt"
}

nacl-x8664() {
  clobber
  basic-setup-nacl "x86-64"
  build-and-run-all "SetupNaclX8664 \
                     SetupNaclX8664Opt"

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
