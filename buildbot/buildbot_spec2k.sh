#!/bin/bash
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
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

readonly BUILDBOT_PNACL="buildbot/buildbot_pnacl.sh"
readonly UP_DOWN_LOAD="buildbot/file_up_down_load.sh"

SPEC_BASE="tests/spec2k"
readonly ARCHIVE_NAME=$(${SPEC_BASE}/run_all.sh GetTestArchiveName)

readonly NAME_ARM_TRY_UPLOAD=$(${BUILDBOT_PNACL} NAME_ARM_TRY_UPLOAD)
readonly NAME_ARM_TRY_DOWNLOAD=$(${BUILDBOT_PNACL} NAME_ARM_TRY_DOWNLOAD)
readonly NAME_ARM_UPLOAD=$(${BUILDBOT_PNACL} NAME_ARM_UPLOAD)
readonly NAME_ARM_DOWNLOAD=$(${BUILDBOT_PNACL} NAME_ARM_DOWNLOAD)

# If true, terminate script when first error is encountered.
FAIL_FAST=${FAIL_FAST:-false}
RETCODE=0

# Print the number of tests being run for the buildbot status output
testcount() {
  local tests="$1"
  if [[ ${tests} == "all" ]]; then
    echo "all"
  else
    echo ${tests} | wc -w
  fi
}

# called when a commands invocation fails
handle-error() {
  RETCODE=1
  echo "@@@STEP_FAILURE@@@"
  if ${FAIL_FAST} ; then
    echo "FAIL_FAST enabled"
    exit 1
  fi
}

######################################################################
# SCRIPT ACTION
######################################################################

clobber() {
  if [ "${CLOBBER}" == "yes" ] ; then
    rm -rf scons-out
  fi
}

# Make up for the toolchain tarballs not quite being a full SDK
# Also clean the SPEC dir (that step is here because it should
# not be run on hw bots which download rather than build binaries)
build-prerequisites() {
  echo "@@@BUILD_STEP build prerequisites [$*] @@@"
  pushd ${SPEC_BASE}
  ./run_all.sh BuildPrerequisites "$@"
  ./run_all.sh CleanBenchmarks
  ./run_all.sh PopulateFromSpecHarness "${SPEC_HARNESS}"
  popd
}

build-tests() {
  local setups="$1"
  local tests="$2"
  local timed="$3" # Do timing and size measurements
  local compile_repetitions="$4"
  local count=$(testcount "${tests}")

  pushd ${SPEC_BASE}
  for setup in ${setups}; do
    echo "@@@BUILD_STEP spec2k build [${setup}] [${count} tests]@@@"
    MAKEOPTS=-j8 \
    SPEC_COMPILE_REPETITIONS=${compile_repetitions} \
      ./run_all.sh BuildBenchmarks ${timed} ${setup} train ${tests} || \
        handle-error
  done
  popd
}

run-tests() {
  local setups="$1"
  local tests="$2"
  local timed="$3"
  local run_repetitions="$4"
  local count=$(testcount "${tests}")

  pushd ${SPEC_BASE}
  for setup in ${setups}; do
    echo "@@@BUILD_STEP spec2k run [${setup}] [${count} tests]@@@"
    if [ ${timed} == "1" ]; then
      SPEC_RUN_REPETITIONS=${run_repetitions} \
        ./run_all.sh RunTimedBenchmarks ${setup} train ${tests} || \
          handle-error
    else
      ./run_all.sh RunBenchmarks ${setup} train ${tests} || \
        handle-error
    fi
  done
  popd
}

upload-test-binaries() {
  local tests="$1"
  local try="$2" # set to "try" if this is a try run

  pushd ${SPEC_BASE}
  echo "@@@BUILD_STEP spec2k archive@@@"
  ./run_all.sh PackageArmBinaries ${tests}
  popd
  echo "@@@BUILD_STEP spec2k upload@@@"
  if [[ ${try} == "try" ]]; then
    ${UP_DOWN_LOAD} UploadArmBinariesForHWBotsTry ${NAME_ARM_TRY_UPLOAD} \
      ${ARCHIVE_NAME}
  else
    ${UP_DOWN_LOAD} UploadArmBinariesForHWBots ${NAME_ARM_UPLOAD} \
      ${ARCHIVE_NAME}
  fi
}

download-test-binaries() {
  local try="$1"
  echo "@@@BUILD_STEP spec2k download@@@"
  if [[ ${try} == "try" ]]; then
    ${UP_DOWN_LOAD} DownloadArmBinariesForHWBotsTry ${NAME_ARM_TRY_DOWNLOAD} \
      ${ARCHIVE_NAME}
  else
    ${UP_DOWN_LOAD} DownloadArmBinariesForHWBots ${NAME_ARM_DOWNLOAD} \
      ${ARCHIVE_NAME}
  fi
  echo "@@@BUILD_STEP spec2k untar@@@"
  pushd ${SPEC_BASE}
  ./run_all.sh UnpackArmBinaries
  popd
}

######################################################################
# NOTE: trybots only runs a subset of the the spec2k tests
# TODO: elminate this long running bot in favor per arch sharded bots
pnacl-trybot-arm-qemu() {
  clobber
  build-prerequisites "arm" "bitcode"
  build-tests SetupPnaclArmOpt "${TRYBOT_TESTS}" 0 1
  run-tests SetupPnaclArmOpt "${TRYBOT_TESTS}" 0 1
}

pnacl-trybot-arm-buildonly() {
  clobber
  build-prerequisites "arm" "bitcode" "arm-ncval-core"
  ${BUILDBOT_PNACL} archive-for-hw-bots "${NAME_ARM_TRY_UPLOAD}" try
  build-tests SetupPnaclPexeOpt "${TRYBOT_TESTS}" 0 1
  upload-test-binaries "${TRYBOT_TESTS}" try
}

pnacl-trybot-arm-hw() {
  clobber
  ${BUILDBOT_PNACL} unarchive-for-hw-bots "${NAME_ARM_TRY_DOWNLOAD}" try
  download-test-binaries try
  build-tests SetupPnaclTranslatorArmOptHW "${TRYBOT_TESTS}" 0 1
  run-tests SetupPnaclTranslatorArmOptHW "${TRYBOT_TESTS}" 0 1
  pushd ${SPEC_BASE};
  ./run_all.sh TimeValidation SetupPnaclTranslatorArmOptHW "${TRYBOT_TESTS}" ||\
    handle-error
  popd
}

pnacl-trybot-x8632() {
  clobber
  build-prerequisites "x86-32" "bitcode"
  build-tests SetupPnaclX8632Opt "${TRYBOT_TESTS}" 0 1
  run-tests SetupPnaclX8632Opt "${TRYBOT_TESTS}" 0 1
  build-tests SetupPnaclTranslatorX8632Opt "${TRYBOT_TRANSLATOR_TESTS}" 0 1
  run-tests SetupPnaclTranslatorX8632Opt "${TRYBOT_TRANSLATOR_TESTS}" 0 1
}

pnacl-trybot-x8664() {
  clobber
  build-prerequisites "x86-64" "bitcode"
  build-tests SetupPnaclX8664Opt "${TRYBOT_TESTS}" 0 1
  run-tests SetupPnaclX8664Opt "${TRYBOT_TESTS}" 0 1
  build-tests SetupPnaclTranslatorX8664Opt "${TRYBOT_TRANSLATOR_TESTS}" 0 1
  run-tests SetupPnaclTranslatorX8664Opt "${TRYBOT_TRANSLATOR_TESTS}" 0 1
}

# We probably will not keep a qemu bot on the waterfall, but will keep this
# to test locally
pnacl-arm-qemu() {
  clobber
  build-prerequisites "arm" "bitcode"
  # arm takes a long time and we do not have sandboxed tests working
  build-tests SetupPnaclArmOpt all 1 1
  run-tests SetupPnaclArmOpt all 1 1
}

pnacl-arm-buildonly() {
  clobber
  build-prerequisites "arm" "bitcode"
  ${BUILDBOT_PNACL} archive-for-hw-bots "${NAME_ARM_UPLOAD}" regular
  build-tests SetupPnaclPexeOpt all 0 1
  upload-test-binaries all regular
}

pnacl-arm-hw() {
  clobber
  ${BUILDBOT_PNACL} unarchive-for-hw-bots "${NAME_ARM_DOWNLOAD}" regular
  download-test-binaries regular
  build-tests SetupPnaclTranslatorArmOptHW all 1 1
  run-tests SetupPnaclTranslatorArmOptHW all 1 1
}

pnacl-x8664() {
  clobber
  build-prerequisites "x86-64" "bitcode"
  local setups="SetupPnaclX8664 \
               SetupPnaclX8664Opt \
               SetupPnaclTranslatorX8664 \
               SetupPnaclTranslatorX8664Opt"
  build-tests "${setups}" all 1 3
  run-tests "${setups}" all 1 3
}

pnacl-x8632() {
  clobber
  build-prerequisites "x86-32" "bitcode"
  local setups="SetupPnaclX8632 \
                SetupPnaclX8632Opt \
                SetupPnaclTranslatorX8632 \
                SetupPnaclTranslatorX8632Opt"
  build-tests "${setups}" all 1 3
  run-tests "${setups}" all 1 3
}

nacl-x8632() {
  clobber
  build-prerequisites "x86-32" ""
  local setups="SetupNaclX8632 \
                SetupNaclX8632Opt"
  build-tests "${setups}" all 1 3
  run-tests "${setups}" all 1 3
}

nacl-x8664() {
  clobber
  build-prerequisites "x86-64" ""
  local setups="SetupNaclX8664 \
                SetupNaclX8664Opt"
  build-tests "${setups}" all 1 3
  run-tests "${setups}" all 1 3
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
