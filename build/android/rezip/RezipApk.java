// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.Enumeration;
import java.util.List;
import java.util.jar.JarEntry;
import java.util.jar.JarFile;
import java.util.jar.JarOutputStream;
import java.util.regex.Pattern;
import java.util.zip.CRC32;

/**
 * Command line tool used to page align non-compressed libraries (*.so) in APK files.
 * Tool is designed so that running SignApk and/or zipalign on the resulting APK does not
 * break the page alignment.
 */
class RezipApk {
    // Alignment to use for non-compressed files (must match zipalign).
    private static final int ALIGNMENT = 4;

    // Alignment to use for non-compressed *.so files
    private static final int LIBRARY_ALIGNMENT = 4096;

    // Files matching this pattern are not copied to the output when adding alignment.
    // When reordering and verifying the APK they are copied to the end of the file.
    private static Pattern sMetaFilePattern =
            Pattern.compile("^(META-INF/((.*)[.](SF|RSA|DSA)|com/android/otacert))|(" +
                            Pattern.quote(JarFile.MANIFEST_NAME) + ")$");

    /**
     * Wraps another output stream, counting the number of bytes written.
     */
    private static class CountingOutputStream extends OutputStream {
        private long mCount = 0;
        private OutputStream mOut;

        public CountingOutputStream(OutputStream out) {
            this.mOut = out;
        }

        /** Returns the number of bytes written. */
        public long getCount() {
            return mCount;
        }

        @Override public void write(byte[] b, int off, int len) throws IOException {
            mOut.write(b, off, len);
            mCount += len;
        }

        @Override public void write(int b) throws IOException {
            mOut.write(b);
            mCount++;
        }

        @Override public void close() throws IOException {
            mOut.close();
        }

        @Override public void flush() throws IOException {
            mOut.flush();
        }
    }

    /**
     * Sort filenames in natural string order, except that filenames matching
     * the meta-file pattern are always after other files. This is so the manifest
     * and signature are at the end of the file after any alignment file.
     */
    private static class FilenameComparator implements Comparator<String> {
        @Override
        public int compare(String o1, String o2) {
            boolean o1Matches = sMetaFilePattern.matcher(o1).matches();
            boolean o2Matches = sMetaFilePattern.matcher(o2).matches();
            if (o1Matches != o2Matches) {
                return o1Matches ? 1 : -1;
            } else {
                return o1.compareTo(o2);
            }
        }
    }

    // Build an ordered list of filenames. Using the same deterministic ordering used
    // by SignApk. If omitMetaFiles is true do not include the META-INF files.
    private static List<String> orderFilenames(JarFile jar, boolean omitMetaFiles) {
        List<String> names = new ArrayList<String>();
        for (Enumeration<JarEntry> e = jar.entries(); e.hasMoreElements(); ) {
            JarEntry entry = e.nextElement();
            if (entry.isDirectory()) {
                continue;
            }
            if (omitMetaFiles &&
                sMetaFilePattern.matcher(entry.getName()).matches()) {
                continue;
            }
            names.add(entry.getName());
        }

        // We sort the input entries by name. When present META-INF files
        // are sorted to the end.
        Collections.sort(names, new FilenameComparator());
        return names;
    }

    /**
     * Add a zero filled alignment file at this point in the zip file,
     * The added file will be added before |name| and after |prevName|.
     * The size of the alignment file is such that the location of the
     * file |name| will be on a LIBRARY_ALIGNMENT boundary.
     *
     * Note this arrangement is devised so that running SignApk and/or zipalign on the resulting
     * file will not alter the alignment.
     *
     * @param offset number of bytes into the output file at this point.
     * @param timestamp time in millis since the epoch to include in the header.
     * @param name the name of the library filename.
     * @param prevName the name of the previous file in the archive (or null).
     * @param out jar output stream to write the alignment file to.
     *
     * @throws IOException if the output file can not be written.
     */
    private static void addAlignmentFile(
            long offset, long timestamp, String name, String prevName,
            JarOutputStream out) throws IOException {

        // Compute the start and alignment of the library, as if it was next.
        int headerSize = JarFile.LOCHDR + name.length();
        long libOffset = offset + headerSize;
        int libNeeded = LIBRARY_ALIGNMENT - (int) (libOffset % LIBRARY_ALIGNMENT);
        if (libNeeded == LIBRARY_ALIGNMENT) {
            // Already aligned, no need to added alignment file.
            return;
        }

        // Check that there is not another file between the library and the
        // alignment file.
        String alignName = name.substring(0, name.length() - 2) + "align";
        if (prevName != null && prevName.compareTo(alignName) >= 0) {
            throw new UnsupportedOperationException(
                "Unable to insert alignment file, because there is "
                + "another file in front of the file to be aligned. "
                + "Other file: " + prevName + " Alignment file: " + alignName);
        }

        // Compute the size of the alignment file header.
        headerSize = JarFile.LOCHDR + alignName.length();
        // We are going to add an alignment file of type STORED. This file
        // will itself induce a zipalign alignment adjustment.
        int extraNeeded =
                (ALIGNMENT - (int) ((offset + headerSize) % ALIGNMENT)) % ALIGNMENT;
        headerSize += extraNeeded;

        if (libNeeded < headerSize + 1) {
            // The header was bigger than the alignment that we need, add another page.
            libNeeded += LIBRARY_ALIGNMENT;
        }
        // Compute the size of the alignment file.
        libNeeded -= headerSize;

        // Build the header for the alignment file.
        byte[] zeroBuffer = new byte[libNeeded];
        JarEntry alignEntry = new JarEntry(alignName);
        alignEntry.setMethod(JarEntry.STORED);
        alignEntry.setSize(libNeeded);
        alignEntry.setTime(timestamp);
        CRC32 crc = new CRC32();
        crc.update(zeroBuffer);
        alignEntry.setCrc(crc.getValue());

        if (extraNeeded != 0) {
            alignEntry.setExtra(new byte[extraNeeded]);
        }

        // Output the alignment file.
        out.putNextEntry(alignEntry);
        out.write(zeroBuffer);
        out.closeEntry();
        out.flush();
    }

    /**
     * Copy the contents of the input APK file to the output APK file. Uncompressed files
     * will be aligned in the output stream. Uncompressed native code libraries (*.so)
     * will be aligned on a page boundary. Page alignment is implemented by adding a
     * zero filled file, regular alignment is implemented by adding a zero filled extra
     * field to the zip file header. Care is take so that the output generated in the
     * same way as SignApk. This is important so that running SignApk and zipalign on
     * the output does not break the page alignment. The archive may not contain a "*.apk"
     * as SignApk has special nested signing logic that we do not support.
     *
     * @param in The input APK File.
     * @param out The output APK stream.
     * @param countOut Counting output stream (to measure the current offset).
     * @param addAlignment Whether to add the alignment file or just check.
     *
     * @throws IOException if the output file can not be written.
     */
    private static void copyAndAlignFiles(
            JarFile in, JarOutputStream out, CountingOutputStream countOut,
            boolean addAlignment) throws IOException {

        List<String> names = orderFilenames(in, addAlignment);
        long timestamp = System.currentTimeMillis();
        byte[] buffer = new byte[4096];
        boolean firstEntry = true;
        String prevName = null;
        for (String name : names) {
            JarEntry inEntry = in.getJarEntry(name);
            JarEntry outEntry = null;
            if (name.endsWith(".apk")) {
                throw new UnsupportedOperationException(
                    "Nested APKs are not supported: " + name);
            }
            if (inEntry.getMethod() == JarEntry.STORED) {
                // Preserve the STORED method of the input entry.
                outEntry = new JarEntry(inEntry);
                outEntry.setExtra(null);
            } else {
                // Create a new entry so that the compressed len is recomputed.
                outEntry = new JarEntry(name);
            }
            outEntry.setTime(timestamp);

            long offset = countOut.getCount();
            if (firstEntry) {
                // The first entry in a jar file has an extra field of
                // four bytes that you can't get rid of; any extra
                // data you specify in the JarEntry is appended to
                // these forced four bytes.  This is JAR_MAGIC in
                // JarOutputStream; the bytes are 0xfeca0000.
                firstEntry = false;
                offset += 4;
            }
            if (inEntry.getMethod() == JarEntry.STORED) {
                if (name.endsWith(".so")) {
                    if (addAlignment) {
                        addAlignmentFile(offset, timestamp, name, prevName, out);
                    }
                    // We check that we did indeed get to a page boundary.
                    offset = countOut.getCount() + JarFile.LOCHDR + name.length();
                    if ((offset % LIBRARY_ALIGNMENT) != 0) {
                        throw new AssertionError(
                            "Library was not page aligned when verifying page alignment. "
                            + "Library name: " + name + " Expected alignment: " + LIBRARY_ALIGNMENT
                            + "Offset: " + offset + " Error: " + (offset % LIBRARY_ALIGNMENT));
                    }
                } else {
                    offset += JarFile.LOCHDR + name.length();
                    int needed = (ALIGNMENT - (int) (offset % ALIGNMENT)) % ALIGNMENT;
                    if (needed != 0) {
                        outEntry.setExtra(new byte[needed]);
                    }
                }
            }
            out.putNextEntry(outEntry);

            int num;
            InputStream data = in.getInputStream(inEntry);
            while ((num = data.read(buffer)) > 0) {
                out.write(buffer, 0, num);
            }
            out.closeEntry();
            out.flush();
            prevName = name;
        }
    }

    private static void usage() {
        System.err.println("Usage: prealignapk (addalignment|reorder) input.apk output.apk");
        System.err.println("  addalignment - adds alignment file removes manifest and signature");
        System.err.println("  reorder      - re-creates canonical ordering and checks alignment");
        System.exit(2);
    }

    public static void main(String[] args) throws IOException {
        if (args.length != 3) usage();

        boolean addAlignment = false;
        if (args[0].equals("addalignment")) {
            addAlignment = true;
        } else if (args[0].equals("reorder")) {
            addAlignment = false;
        } else {
            usage();
        }

        String inputFilename = args[1];
        String outputFilename = args[2];

        JarFile inputJar = null;
        FileOutputStream outputFile = null;

        try {
            inputJar = new JarFile(new File(inputFilename), true);
            outputFile = new FileOutputStream(outputFilename);

            CountingOutputStream outCount = new CountingOutputStream(outputFile);
            JarOutputStream outputJar = new JarOutputStream(outCount);

            // Match the compression level used by SignApk.
            outputJar.setLevel(9);

            copyAndAlignFiles(inputJar, outputJar, outCount, addAlignment);
            outputJar.close();
        } finally {
            if (inputJar != null) inputJar.close();
            if (outputFile != null) outputFile.close();
        }
    }
}
