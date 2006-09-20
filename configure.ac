dnl Copyright (C) 2006 Google Inc.
dnl
dnl Licensed under the Apache License, Version 2.0 (the "License");
dnl you may not use this file except in compliance with the License.
dnl You may obtain a copy of the License at
dnl
dnl     http://www.apache.org/licenses/LICENSE-2.0
dnl
dnl Unless required by applicable law or agreed to in writing, software
dnl distributed under the License is distributed on an "AS IS" BASIS,
dnl WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
dnl See the License for the specific language governing permissions and
dnl limitations under the License.


AC_PREREQ(2.57)

AC_INIT(airbag, 0.1, opensource@google.com)
dnl Sanity check: the argument is just a file that should exist.
AC_CONFIG_SRCDIR(README)
AC_CONFIG_AUX_DIR(autotools)

AM_INIT_AUTOMAKE(subdir-objects)
AM_CONFIG_HEADER(src/config.h)

AC_PROG_CC
AC_PROG_CPP
AC_PROG_CXX

AC_PROG_LIBTOOL
AC_SUBST(LIBTOOL_DEPS)

AC_HEADER_STDC

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
