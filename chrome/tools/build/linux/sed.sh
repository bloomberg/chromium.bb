#!/bin/bash

# Does the equivalent of
#   sed -e A -e B infile > outfile
# in a world where doing it from gyp eats the redirection.

infile="$1"
outfile="$2"
shift 2

sed "$@" "$infile" > "$outfile"
