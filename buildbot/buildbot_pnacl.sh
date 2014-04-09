#!/bin/bash
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -o xtrace
set -o nounset
set -o errexit

# Tell build.sh and test.sh that we're a bot.
export PNACL_BUILDBOT=true
# Make TC bots print all log output to console (this variable only affects
# build.sh output)
export PNACL_VERBOSE=true

# This affects trusted components of gyp and scons builds.
# The setting OPT reuslts in optimized/release trusted executables,
# the setting DEBUG results in unoptimized/debug trusted executables
BUILD_MODE_HOST=OPT
# If true, terminate script when first scons error is encountered.
FAIL_FAST=${FAIL_FAST:-true}
# This remembers when any build steps failed, but we ended up continuing.
RETCODE=0
# For scons builds an empty target indicates all targets should be built.
# This does not run any tests, though.
# Large tests are not included in this group because they cannot be run
# in parallel (they can do things like bind to local TCP ports). Large
# tests are run separately in the functions below.
readonly SCONS_EVERYTHING=""
readonly SCONS_S_M="small_tests medium_tests"
readonly SCONS_S_M_IRT="small_tests_irt medium_tests_irt"
# subset of tests used on toolchain builders
readonly SCONS_TC_TESTS="small_tests medium_tests"

readonly SCONS_COMMON="./scons --verbose bitcode=1"
readonly UP_DOWN_LOAD="buildbot/file_up_down_load.sh"
# This script is used by toolchain bots (i.e. tc-xxx functions)
readonly PNACL_BUILD="pnacl/build.sh"
readonly LLVM_TEST="pnacl/scripts/llvm-test.py"
readonly DRIVER_TESTS="pnacl/driver/tests/driver_tests.py"
readonly ACCEPTABLE_TOOLCHAIN_SIZE_MB=80

setup-goma() {
  echo "@@@BUILD_STEP goma_setup@@@"
  if /b/build/goma/goma_ctl.sh ensure_start ; then
    PATH=/b/build/goma:$PATH
    export PNACL_CONCURRENCY_HOST=100
  else
    # For now, don't make the bot go red if goma fails to start, just fall back
    echo "@@@STEP_WARNINGS@@@"
  fi
}

tc-clobber() {
  local toolchain_dir=$1
  local toolchain_name=$2
  local clobber_translators=$3

  echo @@@BUILD_STEP tc_clobber@@@
  rm -rf ${toolchain_dir}/${toolchain_name}
  rm -rf ${toolchain_dir}/test-log
  rm -rf pnacl*.tgz pnacl/pnacl*.tgz
  if ${clobber_translators} ; then
      rm -rf ${toolchain_dir}/pnacl_translator
  fi
}

tc-show-config() {
  echo @@@BUILD_STEP show_config@@@
  ${PNACL_BUILD} show-config
}

tc-compile-toolchain() {
  local build_fat=$1
  echo @@@BUILD_STEP compile_toolchain@@@
  ${PNACL_BUILD} clean
  # Instead of using build.sh's sync-sources, the sources are now synced by
  # toolchain_build_pnacl.py.
  ${PNACL_BUILD} llvm-link-clang
  ${PNACL_BUILD} newlib-nacl-headers
  if ${build_fat}; then
    HOST_ARCH=x86_32 ${PNACL_BUILD} build-all
    HOST_ARCH=x86_64 ${PNACL_BUILD} build-host
    HOST_ARCH=x86_64 ${PNACL_BUILD} driver
  else
    ${PNACL_BUILD} build-all
  fi
  ${PNACL_BUILD} tarball pnacl-toolchain.tgz
  chmod a+r pnacl-toolchain.tgz

  # Size sanity check
  local byte_size=$(wc -c < pnacl-toolchain.tgz)
  local max_size=$((1024 * 1024 * ${ACCEPTABLE_TOOLCHAIN_SIZE_MB}))
  if ${build_fat}; then
    # Allow an extra 33% tarball size for fat toolchains
    max_size=$((1024 * 1024 * ${ACCEPTABLE_TOOLCHAIN_SIZE_MB} * 4 / 3))
  fi

  if [[ ${byte_size} -gt ${max_size} ]] ; then
      echo "ERROR: toolchain tarball is too large: ${byte_size} > ${max_size}"
      handle-error
  fi
}

tc-untar-toolchain() {
  local toolchain_dir=$1
  local toolchain_name=$2
  echo @@@BUILD_STEP untar_toolchain@@@
  # Untar to ensure we can and to place the toolchain where the main build
  # expects it to be.
  rm -rf ${toolchain_dir}/${toolchain_name}
  mkdir -p ${toolchain_dir}/${toolchain_name}
  tar xfz pnacl-toolchain.tgz -C "${toolchain_dir}/${toolchain_name}"
}

tc-build-translator() {
  echo @@@BUILD_STEP compile_translator@@@
  ${PNACL_BUILD} translator-clean-all
  ${PNACL_BUILD} translator-all
}

tc-archive() {
  local label=$1
  echo @@@BUILD_STEP archive_toolchain@@@
  ${UP_DOWN_LOAD} UploadToolchainTarball ${BUILDBOT_GOT_REVISION} \
    ${label} pnacl-toolchain.tgz
}

tc-prune-translator-pexes() {
  echo @@@BUILD_STEP prune_translator_pexe@@@
  ${PNACL_BUILD} translator-prune
}

tc-archive-translator() {
  echo @@@BUILD_STEP archive_translator@@@
  ${PNACL_BUILD} translator-tarball pnacl-translator.tgz
  ${UP_DOWN_LOAD} UploadToolchainTarball ${BUILDBOT_GOT_REVISION} \
      pnacl_translator pnacl-translator.tgz
}

tc-build-all() {
  local toolchain_dir=$1
  local toolchain_name=$2
  local label=$3
  local is_try=$4
  local build_translator=$5

  # Tell build.sh and test.sh that we're a bot.
  export PNACL_BUILDBOT=true
  # Tells build.sh to prune the install directory (for release).
  export PNACL_PRUNE=true

  clobber
  tc-clobber ${toolchain_dir} ${toolchain_name} ${build_translator}

  # Run checkdeps so that the PNaCl toolchain trybots catch mistakes
  # that would cause the normal NaCl bots to fail.
  echo "@@@BUILD_STEP checkdeps @@@"
  python tools/checkdeps/checkdeps.py

  tc-show-config
  # For now only linux64 (which also builds the translator) builds a fat
  # toolchain, so just use build_translator to control fat toolchain build
  tc-compile-toolchain ${build_translator}
  tc-untar-toolchain ${toolchain_dir} ${toolchain_name}
  if ! ${is_try} ; then
    tc-archive ${label}
  fi

  # NOTE: only one bot needs to do this
  if ${build_translator} ; then
    tc-build-translator
    if ! ${is_try} ; then
      tc-prune-translator-pexes
      tc-archive-translator
    fi
  fi

}


# extract the relevant scons flags for reporting
relevant() {
  for i in "$@" ; do
    case $i in
      use_sandboxed_translator=1)
        echo -n "sbtc "
        ;;
      do_not_run_tests=1)
        echo -n "no_tests "
        ;;
      pnacl_generate_pexe=0)
        echo -n "no_pexe "
        ;;
      translate_fast=1)
        echo -n "fast "
        ;;
      --nacl_glibc)
        echo -n "glibc "
        ;;
    esac
  done
}

# called when a scons invocation fails
handle-error() {
  RETCODE=1
  echo "@@@STEP_FAILURE@@@"
  if ${FAIL_FAST} ; then
    echo "FAIL_FAST enabled"
    exit 1
  fi
}

# Clear out object, and temporary directories.
clobber() {
  echo "@@@BUILD_STEP clobber@@@"
  rm -rf scons-out ../xcodebuild ../out
}

# Generate filenames for arm bot uploads and downloads
NAME_ARM_UPLOAD() {
  echo -n "${BUILDBOT_BUILDERNAME}/${BUILDBOT_GOT_REVISION}"
}

NAME_ARM_DOWNLOAD() {
  echo -n "${BUILDBOT_TRIGGERED_BY_BUILDERNAME}/${BUILDBOT_GOT_REVISION}"
}

NAME_ARM_TRY_UPLOAD() {
  echo -n "${BUILDBOT_BUILDERNAME}/"
  echo -n "${BUILDBOT_SLAVENAME}/"
  echo -n "${BUILDBOT_BUILDNUMBER}"
}
NAME_ARM_TRY_DOWNLOAD() {
  echo -n "${BUILDBOT_TRIGGERED_BY_BUILDERNAME}/"
  echo -n "${BUILDBOT_TRIGGERED_BY_SLAVENAME}/"
  echo -n "${BUILDBOT_TRIGGERED_BY_BUILDNUMBER}"
}


prune-scons-out() {
  find scons-out/ \
    \( -name '*.o' -o -name '*.bc' -o -name 'test_results' \) \
    -print0 | xargs -0 rm -rf
}

# Tar up the executables which are shipped to the arm HW bots
archive-for-hw-bots() {
  local name=$1
  local try=$2

  echo "@@@BUILD_STEP tar_generated_binaries@@@"
  prune-scons-out

  # delete nexes from pexe mode directories to force translation
  # TODO(dschuff) enable this once we can translate on the hw bots
  #find scons-out/*pexe*/ -name '*.nexe' -print0 | xargs -0 rm -f
  tar cvfz arm-scons.tgz scons-out/*arm*

  echo "@@@BUILD_STEP archive_binaries@@@"
  if [[ ${try} == "try" ]] ; then
    ${UP_DOWN_LOAD} UploadArmBinariesForHWBotsTry ${name} arm-scons.tgz
  else
    ${UP_DOWN_LOAD} UploadArmBinariesForHWBots ${name} arm-scons.tgz
  fi
}

# Untar archived executables for HW bots
unarchive-for-hw-bots() {
  local name=$1
  local try=$2

  echo "@@@BUILD_STEP fetch_binaries@@@"
  if [[ ${try} == "try" ]] ; then
    ${UP_DOWN_LOAD} DownloadArmBinariesForHWBotsTry ${name} arm-scons.tgz
  else
    ${UP_DOWN_LOAD} DownloadArmBinariesForHWBots ${name} arm-scons.tgz
  fi

  echo "@@@BUILD_STEP untar_binaries@@@"
  rm -rf scons-out/
  tar xvfz arm-scons.tgz --no-same-owner
}

# Build with gyp - this only exercises the trusted TC and hence this only
# makes sense to run for ARM.
gyp-arm-build() {
  local gypmode="Release"
  if [ "${BUILD_MODE_HOST}" = "DEBUG" ] ; then
      gypmode="Debug"
  fi
  local toolchain_dir=native_client/toolchain/linux_x86/arm_trusted
  local extra="-isystem ${toolchain_dir}/usr/include \
               -Wl,-rpath-link=${toolchain_dir}/lib/arm-linux-gnueabi \
               -L${toolchain_dir}/lib \
               -L${toolchain_dir}/lib/arm-linux-gnueabi \
               -L${toolchain_dir}/usr/lib \
               -L${toolchain_dir}/usr/lib/arm-linux-gnueabi"
  # Setup environment for arm.

  export AR=arm-linux-gnueabi-ar
  export AS=arm-linux-gnueabi-as
  export CC="arm-linux-gnueabi-gcc-4.5 ${extra} "
  export CXX="arm-linux-gnueabi-g++-4.5 ${extra} "
  export LD="arm-linux-gnueabi-g++-4.5 ${extra} "
  export RANLIB=arm-linux-gnueabi-ranlib
  export SYSROOT
  export GYP_DEFINES="target_arch=arm \
    sysroot=${toolchain_dir} \
    linux_use_tcmalloc=0 armv7=1 arm_thumb=1"
  export GYP_GENERATORS=make

  # NOTE: this step is also run implicitly as part of
  #        gclient runhooks --force
  #       it uses the exported env vars so we have to run it again
  #
  echo "@@@BUILD_STEP gyp_configure [${gypmode}]@@@"
  cd ..
  native_client/build/gyp_nacl native_client/build/all.gyp
  cd native_client

  echo "@@@BUILD_STEP gyp_compile [${gypmode}]@@@"
  make -C .. -k -j8 V=1 BUILDTYPE=${gypmode}
}

build-sbtc-prerequisites() {
  local platform=$1
  # Sandboxed translators currently only require irt_core since they do not
  # use PPAPI.
  ${SCONS_COMMON} platform=${platform} sel_ldr sel_universal irt_core
}

# Run a single invocation of scons as its own buildbot stage and handle errors
scons-stage-irt() {
  local platform=$1
  local extra=$2
  local test=$3
  local info="$(relevant ${extra})"
  # TODO(robertm): do we really need both nacl and nacl_irt_test
  local mode="--mode=opt-host,nacl,nacl_irt_test"
  if [ "${BUILD_MODE_HOST}" = "DEBUG" ] ; then
      mode="--mode=dbg-host,nacl,nacl_irt_test"
  fi

  echo "@@@BUILD_STEP scons-irt [${platform}] [${test}] [${info}]@@@"
  ${SCONS_COMMON} ${extra} ${mode} platform=${platform} ${test} || handle-error
}

# Run a single invocation of scons as its own buildbot stage and handle errors
scons-stage-noirt() {
  local platform=$1
  local extra=$2
  local test=$3
  local info="$(relevant ${extra})"
  local mode="--mode=opt-host,nacl"
  if [ "${BUILD_MODE_HOST}" = "DEBUG" ] ; then
      mode="--mode=dbg-host,nacl"
  fi

  echo "@@@BUILD_STEP scons [${platform}] [${test}] [${info}]@@@"
  ${SCONS_COMMON} ${extra} ${mode} platform=${platform} ${test} || handle-error
}

llvm-regression() {
  echo "@@@BUILD_STEP llvm_regression@@@"
  ${LLVM_TEST} --llvm-regression \
    --llvm-buildpath="pnacl/build/llvm_${HOST_ARCH}" || handle-error
}

# QEMU upload bot runs this function, and the hardware download bot runs
# mode-buildbot-arm-hw
mode-buildbot-arm() {
  FAIL_FAST=false
  # "force_emulator=" disables use of QEMU, which enables building
  # tests which don't work under QEMU.
  local qemuflags="-j8 -k do_not_run_tests=1 force_emulator="

  clobber

  gyp-arm-build

  # Sanity check
  scons-stage-noirt "arm" "-j8" "run_hello_world_test"

  # Don't run the rest of the tests on qemu, only build them.
  # QEMU is too flaky for the main waterfall

  # Normal pexe mode tests
  scons-stage-noirt "arm" "${qemuflags}" "${SCONS_EVERYTHING}"
  # This extra step is required to translate the pexes (because translation
  # happens as part of CommandSelLdrTestNacl and not part of the
  # build-everything step)
  scons-stage-noirt "arm" "${qemuflags}" "${SCONS_S_M}"
  # Large tests cannot be run in parallel
  scons-stage-noirt "arm" "${qemuflags} -j1" "large_tests"

  # also run some tests with the irt
  scons-stage-irt "arm" "${qemuflags}" "${SCONS_S_M_IRT}"

  # non-pexe-mode tests
  scons-stage-noirt "arm" "${qemuflags} pnacl_generate_pexe=0" "nonpexe_tests"

  build-sbtc-prerequisites "arm"

  scons-stage-irt "arm" \
    "${qemuflags} use_sandboxed_translator=1 translate_in_build_step=0" \
    "toolchain_tests"
  scons-stage-irt "arm" \
    "${qemuflags} use_sandboxed_translator=1 translate_fast=1 \
       translate_in_build_step=0" \
    "toolchain_tests"
}

mode-buildbot-arm-hw() {
  FAIL_FAST=false
  local hwflags="-j2 -k naclsdk_validate=0 built_elsewhere=1"

  scons-stage-noirt "arm" "${hwflags}" "${SCONS_S_M}"
  # Large tests cannot be run in parallel
  scons-stage-noirt "arm" "${hwflags} -j1" "large_tests"

  # also run some tests with the irt
  scons-stage-irt "arm" "${hwflags}" "${SCONS_S_M_IRT}"

  scons-stage-noirt "arm" "${hwflags} pnacl_generate_pexe=0" "nonpexe_tests"
  scons-stage-irt "arm" \
    "${hwflags} use_sandboxed_translator=1 translate_in_build_step=0" \
    "toolchain_tests"
  scons-stage-irt "arm" \
    "${hwflags} use_sandboxed_translator=1 translate_fast=1 \
       translate_in_build_step=0" \
    "toolchain_tests"
}

mode-trybot-qemu() {
  # Build and actually run the arm tests under qemu, except
  # sandboxed translation. Hopefully that's a good tradeoff between
  # flakiness and cycle time.
  FAIL_FAST=false
  local qemuflags="-j4 -k"
  clobber
  gyp-arm-build

  scons-stage-noirt "arm" "${qemuflags}" "${SCONS_EVERYTHING}"
  # Large tests cannot be run in parallel
  scons-stage-noirt "arm" "${qemuflags} -j1" "${SCONS_S_M} large_tests"

  # also run some tests with the irt
  scons-stage-irt "arm" "${qemuflags}" "${SCONS_S_M_IRT}"

  # non-pexe tests
  scons-stage-noirt "arm" "${qemuflags} pnacl_generate_pexe=0" "nonpexe_tests"
}

mode-buildbot-arm-dbg() {
  BUILD_MODE_HOST=DEDUG
  mode-buildbot-arm
  archive-for-hw-bots $(NAME_ARM_UPLOAD) regular
}

mode-buildbot-arm-opt() {
  mode-buildbot-arm
  archive-for-hw-bots $(NAME_ARM_UPLOAD) regular
}

mode-buildbot-arm-try() {
  mode-buildbot-arm
  archive-for-hw-bots $(NAME_ARM_TRY_UPLOAD) try
}

# NOTE: the hw bots are too slow to build stuff on so we just
#       use pre-built executables
mode-buildbot-arm-hw-dbg() {
  BUILD_MODE_HOST=DEDUG
  unarchive-for-hw-bots $(NAME_ARM_DOWNLOAD)  regular
  mode-buildbot-arm-hw
}

mode-buildbot-arm-hw-opt() {
  unarchive-for-hw-bots $(NAME_ARM_DOWNLOAD)  regular
  mode-buildbot-arm-hw
}

mode-buildbot-arm-hw-try() {
  unarchive-for-hw-bots $(NAME_ARM_TRY_DOWNLOAD)  try
  mode-buildbot-arm-hw
}

# These 2 functions are also suitable for local TC sanity testing.
tc-tests-all() {
  local is_try=$1

  local label="pnaclsdk_mode=custom:toolchain/${PNACL_TOOLCHAIN_DIR}"
  local scons_flags="-k skip_trusted_tests=1 -j8 ${label}"

  llvm-regression

  # newlib
  for arch in x86-32 x86-64 arm; do
    ${DRIVER_TESTS} --platform="$arch"
    scons-stage-noirt "$arch" "${scons_flags}" "${SCONS_TC_TESTS}"
    # Large tests cannot be run in parallel
    scons-stage-noirt "$arch" "${scons_flags} -j1" "large_tests"
    scons-stage-noirt "$arch" "${scons_flags} pnacl_generate_pexe=0" \
        "nonpexe_tests"
  done

  # Small set of sbtc tests w/ and without translate_fast=1.
  scons-stage-irt "x86-32" "${scons_flags} use_sandboxed_translator=1" \
    "toolchain_tests"
  scons-stage-irt "x86-32" \
    "${scons_flags} use_sandboxed_translator=1 translate_fast=1" \
    "toolchain_tests"
  scons-stage-irt "x86-64" "${scons_flags} use_sandboxed_translator=1" \
    "toolchain_tests"
  scons-stage-irt "x86-64" \
    "${scons_flags} use_sandboxed_translator=1 translate_fast=1" \
    "toolchain_tests"
  # Smaller set of sbtc tests for ARM because qemu is flaky.
  scons-stage-irt "arm" "${scons_flags} use_sandboxed_translator=1" \
    "run_hello_world_test"
  scons-stage-irt "arm" \
    "${scons_flags} use_sandboxed_translator=1 translate_fast=1" \
    "run_hello_world_test"

  # Run the GCC torture tests just for x86-32.  Testing a single
  # architecture gives good coverage without taking too long.  We
  # don't test x86-64 here because some of the torture tests fail on
  # the x86-64 toolchain trybot (though not the buildbots, apparently
  # due to a hardware difference:
  # https://code.google.com/p/nativeclient/issues/detail?id=3697).
  echo "@@@BUILD_STEP torture_tests x86-32 @@@"
  tools/toolchain_tester/torture_test.py pnacl x86-32 --verbose \
      --concurrency=8 || handle-error
}

tc-tests-fast() {
  local arch="$1"
  local scons_flags="-k skip_trusted_tests=1"

  llvm-regression
  ${DRIVER_TESTS} --platform="$arch"

  scons-stage-noirt "${arch}" "${scons_flags} -j8" "${SCONS_TC_TESTS}"
  # Large tests cannot be run in parallel
  scons-stage-noirt "${arch}" "${scons_flags} -j1" "large_tests"
  scons-stage-noirt "${arch}" "${scons_flags} -j8 pnacl_generate_pexe=0" \
    "nonpexe_tests"
}

mode-buildbot-tc-x8664-linux() {
  setup-goma
  local is_try=$1
  FAIL_FAST=false
  export PNACL_TOOLCHAIN_DIR=linux_x86/pnacl_newlib
  export PNACL_TOOLCHAIN_LABEL=pnacl_linux_x86
  tc-build-all toolchain/linux_x86 pnacl_newlib \
      ${PNACL_TOOLCHAIN_LABEL} ${is_try} true
  HOST_ARCH=x86_64 tc-tests-all ${is_try}
}

test-nonsfi-mode() {
  echo "@@@BUILD_STEP test translating for nonsfi mode@@@"
  # Test that translation produces an executable without giving an
  # error.  We can't run the resulting Non-SFI Mode nexe yet because
  # there is no standalone loader for such nexes that is available in
  # the NaCl build.
  # TODO(mseaborn): Move some of Chromium's
  # components/nacl/loader/nonsfi/ to the NaCl repo so that we can
  # test this here.
  local out_dir=scons-out/nacl_irt_test-x86-32-pnacl-pexe-clang
  local pexe_path=${out_dir}/obj/tests/hello_world/hello_world.final.pexe
  ./scons -j8 bitcode=1 --mode=nacl,nacl_irt_test ${pexe_path}
  toolchain/linux_x86/pnacl_newlib/bin/pnacl-translate -arch x86-32-nonsfi \
      ${pexe_path} -o /tmp/hellow.nexe
  rm /tmp/hellow.nexe
}

mode-buildbot-tc-x8632-linux() {
  setup-goma
  local is_try=$1
  FAIL_FAST=false
  export PNACL_TOOLCHAIN_DIR=linux_x86/pnacl_newlib
  export PNACL_TOOLCHAIN_LABEL=pnacl_linux_x86
  # For now, just use this bot to test a pure 32 bit build but don't upload
  tc-build-all toolchain/linux_x86 pnacl_newlib \
      ${PNACL_TOOLCHAIN_LABEL} true false
  HOST_ARCH=x86_32 tc-tests-fast "x86-32"

  echo "@@@BUILD_STEP test unsandboxed mode@@@"
  # Test translation to an unsandboxed executable.
  # TODO(mseaborn): Run more tests here when they pass.
  ./scons bitcode=1 pnacl_unsandboxed=1 \
    --mode=nacl_irt_test platform=x86-32 -j8 \
    run_hello_world_test_irt \
    run_irt_futex_test_irt \
    run_thread_test_irt \
    run_float_test_irt \
    run_malloc_realloc_calloc_free_test_irt \
    run_dup_test_irt \
    run_cond_timedwait_test_irt \
    run_syscall_test_irt \
    run_getpid_test_irt

  test-nonsfi-mode
}

mode-buildbot-tc-x8632-mac() {
  local is_try=$1
  FAIL_FAST=false
  export PNACL_TOOLCHAIN_DIR=mac_x86/pnacl_newlib
  export PNACL_TOOLCHAIN_LABEL=pnacl_mac_x86
  # We can't test ARM because we do not have QEMU for Mac.
  # We can't test X86-64 because NaCl X86-64 Mac support is not in good shape.
  tc-build-all toolchain/mac_x86 pnacl_newlib \
      ${PNACL_TOOLCHAIN_LABEL} ${is_try} false
  HOST_ARCH=x86_32 tc-tests-fast "x86-32"

  echo "@@@BUILD_STEP test unsandboxed mode@@@"
  # Test translation to an unsandboxed executable.
  # TODO(mseaborn): Use the same test list as on Linux when the
  # threading tests pass for Mac.
  ./scons bitcode=1 pnacl_unsandboxed=1 \
    --mode=nacl_irt_test platform=x86-32 -j8 run_hello_world_test_irt
}

mode-buildbot-tc-x8664-win() {
  local is_try=$1
  FAIL_FAST=false
  # NOTE: this is a 64bit bot but the TC generated is 32bit
  export PNACL_TOOLCHAIN_DIR=win_x86_pnacl/pnacl_newlib
  export PNACL_TOOLCHAIN_LABEL=pnacl_win_x86
  tc-build-all toolchain/win_x86_pnacl pnacl_newlib \
      ${PNACL_TOOLCHAIN_LABEL} ${is_try} false

  # We can't test ARM because we do not have QEMU for Win.
  HOST_ARCH=x86_32 tc-tests-fast "x86-64"
}



mode-test-all() {
  test-all-newlib "$@"
  test-all-glibc "$@"
}

# NOTE: clobber and toolchain setup to be done manually, since this is for
# testing a locally built toolchain.
# This runs tests concurrently, so may be more difficult to parse logs.
test-all-newlib() {
  local concur=$1

  # turn verbose mode off
  set +o xtrace

  llvm-regression
  ${DRIVER_TESTS} --platform=x86-32
  ${DRIVER_TESTS} --platform=x86-64
  ${DRIVER_TESTS} --platform=arm

  # At least clobber scons-out before building and running the tests though.
  echo "@@@BUILD_STEP clobber@@@"
  rm -rf scons-out

  # First build everything.
  echo "@@@BUILD_STEP scons build @@@"
  local scons_flags="skip_trusted_tests=1 -j${concur}"
  scons-stage-noirt "arm"    "${scons_flags}" "${SCONS_EVERYTHING}"
  scons-stage-noirt "x86-32" "${scons_flags}" "${SCONS_EVERYTHING}"
  scons-stage-noirt "x86-64" "${scons_flags}" "${SCONS_EVERYTHING}"
  # Then run the tests. smoke_tests can be run in parallel but not large_tests
  echo "@@@BUILD_STEP scons smoke_tests @@@"
  scons-stage-noirt "arm"    "${scons_flags}" "smoke_tests"
  scons-stage-noirt "arm"    "${scons_flags} -j1" "large_tests"
  scons-stage-noirt "x86-32" "${scons_flags}" "smoke_tests"
  scons-stage-noirt "x86-32"    "${scons_flags} -j1" "large_tests"
  scons-stage-noirt "x86-64" "${scons_flags}" "smoke_tests"
  scons-stage-noirt "x86-64"    "${scons_flags} -j1" "large_tests"
  # Do some sandboxed translator tests as well.
  scons-stage-irt "x86-32" "${scons_flags} use_sandboxed_translator=1" \
    "toolchain_tests"
  scons-stage-irt "x86-32" \
    "${scons_flags} use_sandboxed_translator=1 translate_fast=1" \
    "toolchain_tests"
  scons-stage-irt "x86-64" "${scons_flags} use_sandboxed_translator=1" \
    "toolchain_tests"
  scons-stage-irt "x86-64" \
    "${scons_flags} use_sandboxed_translator=1 translate_fast=1" \
    "toolchain_tests"
  # Smaller set of sbtc tests for ARM because qemu is flaky.
  scons-stage-irt "arm" "${scons_flags} use_sandboxed_translator=1" \
    "run_hello_world_test"
  scons-stage-irt "arm" \
    "${scons_flags} use_sandboxed_translator=1 translate_fast=1" \
    "run_hello_world_test"
}

test-all-glibc() {
  echo "TODO(pdox): Add GlibC tests"
}

######################################################################
# On Windows, this script is invoked from a batch file.
# The inherited PWD environmental variable is a Windows-style path.
# This can cause problems with pwd and bash. This line fixes it.
cd -P .

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
