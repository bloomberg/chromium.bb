#!/bin/bash

# Script assumed to be run in native_client/
if [[ $(pwd) != */native_client ]]; then
  echo "ERROR: must be run in native_client!"
  exit 1
fi

set -x
set -e
set -u

######################################################################
# SCRIPT CONFIG
######################################################################

CLOBBER=${CLOBBER:-yes}
SCONS_COMMON="./scons --mode=opt-linux bitcode=1 -j8"
SPEC_HARNESS=${SPEC_HARNESS:-/home/chrome-bot/cpu2000-redhat64-ia32}
SETUPS="SetupPnaclArmOpt SetupPnaclX8632Opt SetupPnaclX8664Opt \
        SetupPnaclTranslatorX8632Opt SetupPnaclTranslatorX8664Opt"

# Rough test running time classification for ARM which is our bottleneck
FAST_ARM="176.gcc 181.mcf 197.parser 254.gap"
MEDIUM_ARM="164.gzip 175.vpr 179.art 186.crafty 252.eon 256.bzip2 255.vortex 300.twolf"
SLOW_ARM="177.mesa 183.equake 188.ammp 253.perlbmk"

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

if [ "${CLOBBER}" == "yes" ] ; then
  echo @@@BUILD_STEP clobber@@@
  rm -rf scons-out toolchain compiler hg ../xcodebuild ../sconsbuild ../out \
    src/third_party/nacl_sdk/arm-newlib

  echo @@@BUILD_STEP gclient_runhooks@@@
  gclient runhooks --force
fi

for platform in arm x86-32 x86-64 ; do
  echo @@@BUILD_STEP scons sel_ldr [${platform}]@@@
  ${SCONS_COMMON} platform=${platform} sel_ldr sel_universal
done

cd tests/spec2k/


for setup in ${SETUPS}; do
  echo @@@BUILD_STEP spec2k build [${setup}] [train]@@@
  ./run_all.sh CleanBenchmarks
  ./run_all.sh PopulateFromSpecHarness ${SPEC_HARNESS}
  MAKEOPTS=-j8 ./run_all.sh BuildBenchmarks 0 ${setup} $(TestsToBuild ${setup})

  echo @@@BUILD_STEP spec2k run [${setup}] [train]@@@
  ./run_all.sh RunBenchmarks ${setup} train $(TestsToRun ${setup})
done
