#!/usr/bin/perl -w
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Use: find_files.pl <start-from> [exclude-dir ...]

use strict;
use warnings;
use File::Basename;

my $progname = basename($0);

my $root_dir = shift @ARGV;
my @find_args = ();
while (@ARGV) {
    my $path = shift @ARGV;
    push @find_args, qw'-not ( -path', "*/$path/*", qw'-prune )'
}
push @find_args, qw(-follow -type f -print);

open FIND, '-|', 'find', $root_dir, @find_args
            or die "$progname: Couldn't exec find: $!\n";
my $check_regex = '\.(asm|c(c|pp|xx)?|h(h|pp|xx)?|p(l|m)|xs|sh|php|py(|x)' .
    '|rb|idl|java|el|sc(i|e)|cs|pas|inc|js|pac|html|dtd|xsl|mod|mm?' .
    '|tex|mli?)$';
my @files = ();
while (<FIND>) {
    chomp;
    print "$_\n" unless (-z $_ || !m%$check_regex%);
}
close FIND;
