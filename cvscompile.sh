#!/bin/sh
autoconf
if [ x"$NO_CONFIGURE" = "x" ]; then
    echo " + Running 'configure $@':"
    if [ -z "$*" ]; then
	echo "   ** If you wish to pass arguments to ./configure, please"
        echo "   ** specify them on the command line."
    fi
    ./configure ${1+"$@"} && echo "Now type 'make' to compile" || exit 1
fi

