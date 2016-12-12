#! /bin/sh

srcdir=`dirname "$0"`
test -z "$srcdir" && srcdir=.

ORIGDIR=`pwd`
cd "$srcdir"

autoreconf --force --verbose --install || exit 1
cd "$ORIGDIR" || exit $?

git config --local --get format.subjectPrefix ||
    git config --local format.subjectPrefix "PATCH libdrm" 2>/dev/null

git config --local --get sendemail.to ||
    git config --local sendemail.to "dri-devel@lists.freedesktop.org" 2>/dev/null

if test -z "$NOCONFIGURE"; then
    "$srcdir"/configure "$@"
fi
