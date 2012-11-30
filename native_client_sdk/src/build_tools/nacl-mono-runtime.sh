#!/bin/bash
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -o nounset
set -o errexit

if [[ $# -ne 3 ]]; then
  echo "Usage: $0 [path_to_mono] [build_dir] [install_dir]"
  exit -1
fi

readonly MONO_DIR=$(readlink -f $1)
readonly BUILD_DIR=$(readlink -f $2)
readonly INSTALL_DIR=$(readlink -f $3)
readonly ORIGINAL_CWD=$(pwd)

set +e
if [ -f ${BUILD_DIR}/Makefile ]; then
  cd ${BUILD_DIR}
  make distclean
fi
set -e
cd $ORIGINAL_CWD

case "${TARGET_ARCH}" in
x86-32)
  readonly USE_PNACL=0
  readonly NACL_CROSS_PREFIX_DASH=i686-nacl-
  readonly CONFIG_OPTS="--host=i686-pc-linux-gnu \
                        --build=i686-pc-linux-gnu \
                        --target=i686-pc-linux-gnu"
  readonly LIBDIR=lib32
  readonly CUSTOM_CFLAGS=""
  readonly CUSTOM_LDFLAGS=""
  ;;
x86-64)
  readonly USE_PNACL=0
  readonly NACL_CROSS_PREFIX_DASH=x86_64-nacl-
  readonly CONFIG_OPTS="--host=x86_64-pc-linux-gnu \
                        --build=x86_64-pc-linux-gnu \
                        --target=x86_64-pc-linux-gnu"
  readonly LIBDIR=lib
  readonly CUSTOM_CFLAGS=""
  readonly CUSTOM_LDFLAGS=""
  ;;
arm)
  readonly USE_PNACL=1
  readonly NACL_CROSS_PREFIX_DASH=pnacl-
  readonly PNACL_LINK_ARGS="-arch armv7 -O2"
  readonly CONFIG_OPTS="--host=armv7l-unknown-linux-gnueabi \
                        --build=x86_64-pc-linux-gnu \
                        --target=armv7l-unknown-linux-gnueabi"
  readonly LIBDIR=libarm
  # TODO(olonho): move it to Mono's configure, once nacl target implemented.
  readonly CUSTOM_CFLAGS="\
-D__arm__ \
-D__ARM_ARCH_7__ \
-D__portable_native_client__ \
-DARM_FPU_VFP=1 \
-Dtimezone=_timezone \
-DDISABLE_SOCKETS \
-DDISABLE_ATTACH \
-DUSE_NEWLIB \
"
  readonly CUSTOM_LDFLAGS=${PNACL_LINK_ARGS}
  ;;
esac

# UGLY hack to allow dynamic linking
sed -i -e s/elf_i386/elf_nacl/ -e s/elf_x86_64/elf64_nacl/ \
    ${MONO_DIR}/configure
sed -i -e s/elf_i386/elf_nacl/ -e s/elf_x86_64/elf64_nacl/ \
    ${MONO_DIR}/libgc/configure
sed -i -e s/elf_i386/elf_nacl/ -e s/elf_x86_64/elf64_nacl/ \
    ${MONO_DIR}/eglib/configure

rm -rf ${BUILD_DIR}
mkdir -p ${BUILD_DIR}
cd ${BUILD_DIR}

mkdir -p ${INSTALL_DIR}

if [ ${USE_PNACL} == 1 ]; then
  readonly NACL_BIN_PATH="\
${NACL_SDK_ROOT}/toolchain/linux_x86_pnacl/newlib/bin"
  readonly NACLCC=${NACL_BIN_PATH}/pnacl-clang
  readonly NACLCXX=${NACL_BIN_PATH}/pnacl-clang++
else
  readonly NACL_BIN_PATH=${NACL_SDK_ROOT}/toolchain/linux_x86_glibc/bin
  readonly NACLCC=${NACL_BIN_PATH}/${NACL_CROSS_PREFIX_DASH}gcc
  readonly NACLCXX=${NACL_BIN_PATH}/${NACL_CROSS_PREFIX_DASH}g++
fi
readonly NACLAR=${NACL_BIN_PATH}/${NACL_CROSS_PREFIX_DASH}ar
readonly NACLRANLIB=${NACL_BIN_PATH}/${NACL_CROSS_PREFIX_DASH}ranlib
readonly NACLLD=${NACL_BIN_PATH}/${NACL_CROSS_PREFIX_DASH}ld
readonly NACLOBJDUMP=${NACL_BIN_PATH}/${NACL_CROSS_PREFIX_DASH}objdump
readonly NACLSTRIP=${NACL_BIN_PATH}/${NACL_CROSS_PREFIX_DASH}strip

CFLAGS="-g -O2 -D_POSIX_PATH_MAX=256 -DPATH_MAX=256 $CUSTOM_CFLAGS"
LDFLAGS="$CUSTOM_LDFLAGS"
LIBS="-lnacl_dyncode -lc -lg -lnosys -lpthread"

CC=${NACLCC} CXX=${NACLCXX} LD=${NACLLD} \
STRIP=${NACLSTRIP} AR=${NACLAR} RANLIB=${NACLRANLIB} OBJDUMP=${NACLOBJDUMP} \
PKG_CONFIG_LIBDIR= \
PATH=${NACL_BIN_PATH}:${PATH} \
LIBS="${LIBS}" \
CFLAGS="${CFLAGS}" \
LDFLAGS="${LDFLAGS}" \
${MONO_DIR}/configure ${CONFIG_OPTS} \
  --exec-prefix=${INSTALL_DIR} \
  --libdir=${INSTALL_DIR}/${LIBDIR} \
  --prefix=${INSTALL_DIR} \
  --program-prefix=${NACL_CROSS_PREFIX_DASH} \
  --oldincludedir=${INSTALL_DIR}/include \
  --with-glib=embedded \
  --with-tls=pthread \
  --enable-threads=posix \
  --without-sigaltstack \
  --without-mmap \
  --with-gc=included \
  --enable-nacl-gc \
  --with-sgen=no \
  --enable-nls=no \
  --enable-nacl-codegen \
  --disable-system-aot \
  --enable-shared \
  --disable-parallel-mark \
  --with-static-mono=no

make
make install
