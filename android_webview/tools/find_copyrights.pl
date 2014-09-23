#!/usr/bin/perl -w
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Use: echo filename1.cc ... | find_copyrights.pl
#  or: find_copyrights.pl list_file
#  or: find_files.pl ... | find_copyrights.pl

use strict;
use warnings;
use File::Basename;

sub check_is_generated_file($);
sub start_copyright_parsing();

my $progname = basename($0);

my $generated_file_scan_boundary = 25;
while (<>) {
    chomp;
    my $file = $_;
    my $file_header = '';
    my %copyrights;
    open (F, "<$file") or die "$progname: Unable to access $file\n";
    my $parse_copyright = start_copyright_parsing();
    while (<F>) {
        $file_header .= $_ unless $. > $generated_file_scan_boundary;
        my $copyright_match = $parse_copyright->($_, $.);
        if ($copyright_match) {
            $copyrights{lc("$copyright_match")} = "$copyright_match";
        }
    }
    close(F);
    my $copyright = join(" / ", sort values %copyrights);
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

sub are_within_increasing_progression($$$) {
    my $delta = $_[0] - $_[1];
    return $delta >= 0 && $delta <= $_[2];
}

sub start_copyright_parsing() {
    my $max_line_numbers_proximity = 3;
    # Set up the defaults the way that proximity checks will not succeed.
    my $last_a_item_line_number = -200;
    my $last_b_item_line_number = -100;

    return sub {
        my $line = $_[0];
        my $line_number = $_[1];

        # Remove C / C++ strings to avoid false positives.
        if (index($line, '"') != -1) {
            $line =~ s/"[^"\\]*(?:\\.[^"\\]*)*"//g;
        }

        my $uc_line = uc($line);

        # Record '(a)' and '(b)' last occurences in C++ comments.
        my $cpp_comment_idx = index($uc_line, '//');
        if ($cpp_comment_idx != -1) {
            if (index($uc_line, '(A)') > $cpp_comment_idx) {
                $last_a_item_line_number = $line_number;
            }
            if (index($uc_line, '(B)') > $cpp_comment_idx) {
                $last_b_item_line_number = $line_number;
            }
        }

        # Fast bailout, uses the same patterns as the regexp.
        if (index($uc_line, 'COPYRIGHT') == -1 &&
            index($uc_line, 'COPR.') == -1 &&
            index($uc_line, '\x{00a9}') == -1 &&
            index($uc_line, '\xc2\xa9') == -1) {

            my $c_item_index = index($uc_line, '(C)');
            return '' if ($c_item_index == -1);
            # Filter out 'c' used as a list item inside C++ comments.
            # E.g. "// blah-blah (a) blah\n// blah-blah (b) and (c) blah"
            if ($c_item_index > $cpp_comment_idx &&
                are_within_increasing_progression(
                    $line_number,
                    $last_b_item_line_number,
                    $max_line_numbers_proximity) &&
                are_within_increasing_progression(
                    $last_b_item_line_number,
                    $last_a_item_line_number,
                    $max_line_numbers_proximity)) {
                return '';
            }
        }

        my $copyright_indicator_regex =
            '(?:copyright|copr\.|\x{00a9}|\xc2\xa9|\(c\))';
        my $full_copyright_indicator_regex =
            sprintf '(?:\W|^)%s(?::\s*|\s+)(\w.*)$', $copyright_indicator_regex;
        my $copyright_disindicator_regex =
            '\b(?:info(?:rmation)?|notice|and|or)\b';

        my $copyright = '';
        if ($line =~ m%$full_copyright_indicator_regex%i) {
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
}
