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

# Rough test running time classification for ARM which is our bottleneck
# BUG=http://code.google.com/p/nativeclient/issues/detail?id=2036
# 254.gap disabled for llvm/llvm-gcc 128002/126872

FAST_ARM="176.gcc 181.mcf 197.parser"
MEDIUM_ARM="164.gzip 175.vpr 179.art 186.crafty 252.eon \
            256.bzip2 255.vortex 300.twolf"
SLOW_ARM="177.mesa 183.equake 188.ammp 253.perlbmk"

SPEC_BASE="tests/spec2k"

TestsToBuild() {
  local setup=$1
  case ${setup} in
    SetupPnaclArmOpt)
      # we expect arm to diverge
      echo ${FAST_ARM} 252.eon 179.art
      ;;
    SetupPnaclTranslator*)
      echo 176.gcc
      ;;
    *)
      echo ${FAST_ARM} 252.eon 179.art
      ;;
  esac
}

TestsToRun() {
  local setup=$1
  case ${setup} in
    SetupPnaclArmOpt)
      # we expect arm to diverge
      echo ${FAST_ARM} 252.eon 179.art
      ;;
    SetupPnaclTranslator*)
      echo 176.gcc
      ;;
    *)
      echo ${FAST_ARM} 252.eon 179.art
      ;;
  esac
}

######################################################################
# SCRIPT ACTION
######################################################################

clobber() {
  echo "@@@BUILD_STEP clobber@@@"
  rm -rf scons-out toolchain

  echo "@@@BUILD_STEP gclient_runhooks@@@"
  gclient runhooks --force
}

basic-setup-nacl() {
  local platforms=$1
  build-sel_ldr "${platforms}"
  build-libs "${platforms}"
}

basic-setup-pnacl() {
  local platforms=$1
  build-sel_ldr "${platforms}"
  build-sel_universal "${platforms}"
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

build-libs() {
  local platforms=$1
  shift 1
  for platform in ${platforms} ; do
    echo "@@@BUILD_STEP scons build_lib [${platform}] $* @@@"
    ${SCONS_NACL} platform=${platform} build_lib "$@"
  done
}

build-and-run-some() {
  local harness=$1
  local setups=$2

  pushd ${SPEC_BASE}
  for setup in ${setups}; do
    echo "@@@BUILD_STEP spec2k build [${setup}] [train-some]@@@"
    ./run_all.sh CleanBenchmarks
    ./run_all.sh PopulateFromSpecHarness ${harness}
    MAKEOPTS=-j8 \
      ./run_all.sh BuildBenchmarks 0 ${setup} $(TestsToBuild ${setup})

    echo "@@@BUILD_STEP spec2k run [${setup}] [train-some]@@@"
    ./run_all.sh RunBenchmarks ${setup} train $(TestsToRun ${setup}) || \
      { RETCODE=$? && echo "@@@STEP_FAILURE@@@"; }
  done
  popd
}

build-and-run-all-timed() {
  local harness=$1
  local setups=$2

  pushd ${SPEC_BASE}
  for setup in ${setups}; do
    echo "@@@BUILD_STEP spec2k build [${setup}] [train]@@@"
    ./run_all.sh CleanBenchmarks
    ./run_all.sh PopulateFromSpecHarness ${harness}
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
mode-spec-pnacl-trybot() {
  clobber
  basic-setup-pnacl "arm x86-64 x86-32"
  build-and-run-some ${SPEC_HARNESS} "SetupPnaclArmOpt \
                                      SetupPnaclX8632Opt \
                                      SetupPnaclX8664Opt \
                                      SetupPnaclTranslatorX8632Opt \
                                      SetupPnaclTranslatorX8664Opt"
}

mode-spec-pnacl-trybot-arm() {
  clobber
  basic-setup-pnacl "arm"
  build-and-run-some ${SPEC_HARNESS} "SetupPnaclArmOpt"
}

mode-spec-pnacl-trybot-x8632() {
  clobber
  basic-setup-pnacl "x86-32"
  build-and-run-some ${SPEC_HARNESS} "SetupPnaclX8632Opt \
                                      SetupPnaclTranslatorX8632Opt"
}

mode-spec-pnacl-trybot-x8664() {
  clobber
  basic-setup-pnacl "x86-64"
  build-and-run-some ${SPEC_HARNESS} "SetupPnaclX8664Opt \
                                      SetupPnaclTranslatorX8664Opt"
}


mode-spec-pnacl-arm() {
  clobber
  basic-setup-pnacl "arm"
  # arm takes a long time and we do not have sandboxed tests working
  build-and-run-all-timed ${SPEC_HARNESS} "SetupPnaclArmOpt"
}

mode-spec-pnacl-x8664() {
  clobber
  basic-setup-pnacl "x86-64"
  build-and-run-all-timed ${SPEC_HARNESS} \
                          "SetupPnaclX8664 \
                           SetupPnaclX8664Opt \
                           SetupPnaclTranslatorX8664 \
                           SetupPnaclTranslatorX8664Opt"
}

mode-spec-pnacl-x8632() {
  clobber
  basic-setup-pnacl "x86-32"
  build-and-run-all-timed ${SPEC_HARNESS} \
                          "SetupPnaclX8632 \
                           SetupPnaclX8632Opt \
                           SetupPnaclTranslatorX8632 \
                           SetupPnaclTranslatorX8632Opt"
}

# scheduled to be obsolete
# TODO(robertm): delete this target
mode-spec-nacl() {
  clobber
  basic-setup-nacl "x86-32 x86-64"
  build-and-run-all-timed ${SPEC_HARNESS} \
                          "SetupNaclX8664 \
                           SetupNaclX8664Opt \
                           SetupNaclX8632 \
                           SetupNaclX8632Opt"
}

mode-spec-nacl-x8632() {
  clobber
  basic-setup-nacl "x86-32"
  build-and-run-all-timed ${SPEC_HARNESS} \
                           "SetupNaclX8632 \
                           SetupNaclX8632Opt"
}

mode-spec-nacl-x8664() {
  clobber
  basic-setup-nacl "x86-64"
  build-and-run-all-timed ${SPEC_HARNESS} \
                          "SetupNaclX8664 \
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

eval "$@"

