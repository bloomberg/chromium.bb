#!/usr/bin/perl -w
# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Use: find_copyrights.pl <start-from> [exclude-dir ...]

use strict;
use warnings;
use File::Basename;

sub check_is_generated_file($);
sub parse_copyright($);

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
    push @files, $_ unless (-z $_ || !m%$check_regex%);
}
close FIND;

my $generated_file_scan_boundary = 25;
while (@files) {
    my $file = shift @files;
    my $file_header = '';
    my %copyrights;
    open (F, "<$file") or die "$progname: Unable to access $file\n";
    while (<F>) {
        $file_header .= $_ unless $. > $generated_file_scan_boundary;
        my $copyright_match = parse_copyright($_);
        if ($copyright_match) {
            $copyrights{lc("$copyright_match")} = "$copyright_match";
        }
    }
    close(F);
    my $copyright = join(" / ", values %copyrights);
    print "$file\t";
    if (check_is_generated_file($file_header)) {
        print "GENERATED FILE";
    } else {
        print ($copyright or "*No copyright*");
    }
    print "\n";
}

sub check_is_generated_file($) {
    my $license = uc($_[0]);
    # Remove Python multiline comments to avoid false positives
    if (index($license, '"""') != -1) {
        $license =~ s/"""[^"]*(?:"""|$)//mg;
    }
    if (index($license, "'''") != -1) {
        $license =~ s/'''[^']*(?:'''|$)//mg;
    }
    # Quick checks using index.
    if (index($license, 'ALL CHANGES MADE IN THIS FILE WILL BE LOST') != -1) {
        return 1;
    }
    if (index($license, 'DO NOT EDIT') != -1 ||
        index($license, 'DO NOT DELETE') != -1 ||
        index($license, 'GENERATED') != -1) {
        return ($license =~ /(All changes made in this file will be lost' .
            'DO NOT (EDIT|delete this file)|Generated (at|automatically|data)' .
            '|Automatically generated|\Wgenerated\s+(?:\w+\s+)*file\W)/i);
    }
    return 0;
}

sub parse_copyright($) {
    my $line = $_[0];
    # Remove C / C++ strings to avoid false positives.
    if (index($line, '"') != -1) {
        $line =~ s/"[^"\\]*(?:\\.[^"\\]*)*"//g;
    }

    my $uc_line = uc($line);
    # Fast bailout, uses the same patterns as the regexp.
    return '' if (index($uc_line, 'COPYRIGHT') == -1 &&
                  index($uc_line, 'COPR.') == -1 &&
                  index($uc_line, '\x{00a9}') == -1 &&
                  index($uc_line, '\xc2\xa9') == -1 &&
                  index($uc_line, '(C)') == -1);

    my $copyright_indicator_regex =
        '(?:copyright|copr\.|\x{00a9}|\xc2\xa9|\(c\))';
    my $copyright_disindicator_regex =
        '\b(?:info(?:rmation)?|notice|and|or)\b';

    my $copyright = '';
    if ($line =~ m%\W$copyright_indicator_regex(?::\s*|\s+)(\w.*)$%i) {
        my $match = $1;
        if ($match !~ m%^\s*$copyright_disindicator_regex%i) {
            $match =~ s/([,.])?\s*$//;
            $match =~ s/$copyright_indicator_regex//ig;
            $match =~ s/^\s+//;
            $match =~ s/\s{2,}/ /g;
            $match =~ s/\\@/@/g;
            $copyright = $match;
        }
    }

    return $copyright;
}
