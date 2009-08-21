/*
 * qt-faststart.c, v0.2
 * by Mike Melanson (melanson@pcisys.net)
 * patch by Frank Barchard, to remove last FREE atom.
 * This file is placed in the public domain. Use the program however you
 * see fit.
 *
 * This utility rearranges a Quicktime file such that the moov atom
 * is in front of the data, thus facilitating network streaming.
 *
 * To compile this program, start from the base directory from which you
 * are building FFmpeg and type:
 *  make tools/qt-faststart
 * The qt-faststart program will be built in the tools/ directory. If you
 * do not build the program in this manner, correct results are not
 * guaranteed, particularly on 64-bit platforms.
 * Invoke the program with:
 *  qt-faststart <infile.mov> <outfile.mov>
 *
 * Notes: Quicktime files can come in many configurations of top-level
 * atoms. This utility stipulates that the very last atom in the file needs
 * to be a moov atom. When given such a file, this utility will rearrange
 * the top-level atoms by shifting the moov atom from the back of the file
 * to the front, and patch the chunk offsets along the way. This utility
 * presently only operates on uncompressed moov atoms.
 */

#include <stdio.h>
#include <stdlib.h>
#ifndef _MSC_VER
#include <inttypes.h>
#else
#pragma warning(push,1)
#define PRIu64 "I64u"
#define PRIX64 "I64x"
#define uint64_t unsigned __int64
#define uint32_t unsigned int
#define uint8_t unsigned char
#define ftello _ftelli64
#define fseeko _fseeki64
#endif

#ifdef __MINGW32__
#define fseeko(x,y,z)  fseeko64(x,y,z)
#define ftello(x)      ftello64(x)
#endif

#define BE_16(x) ((((uint8_t*)(x))[0] << 8) | ((uint8_t*)(x))[1])
#define BE_32(x) ((((uint8_t*)(x))[0] << 24) | \
                  (((uint8_t*)(x))[1] << 16) | \
                  (((uint8_t*)(x))[2] << 8) | \
                   ((uint8_t*)(x))[3])
#define BE_64(x) (((uint64_t)(((uint8_t*)(x))[0]) << 56) | \
                  ((uint64_t)(((uint8_t*)(x))[1]) << 48) | \
                  ((uint64_t)(((uint8_t*)(x))[2]) << 40) | \
                  ((uint64_t)(((uint8_t*)(x))[3]) << 32) | \
                  ((uint64_t)(((uint8_t*)(x))[4]) << 24) | \
                  ((uint64_t)(((uint8_t*)(x))[5]) << 16) | \
                  ((uint64_t)(((uint8_t*)(x))[6]) << 8) | \
                  ((uint64_t)((uint8_t*)(x))[7]))

#define BE_FOURCC( ch0, ch1, ch2, ch3 )             \
        ( (uint32_t)(unsigned char)(ch3) |          \
        ( (uint32_t)(unsigned char)(ch2) << 8 ) |   \
        ( (uint32_t)(unsigned char)(ch1) << 16 ) |  \
        ( (uint32_t)(unsigned char)(ch0) << 24 ) )

#define QT_ATOM BE_FOURCC
/* top level atoms */
#define FREE_ATOM QT_ATOM('f', 'r', 'e', 'e')
#define JUNK_ATOM QT_ATOM('j', 'u', 'n', 'k')
#define MDAT_ATOM QT_ATOM('m', 'd', 'a', 't')
#define MOOV_ATOM QT_ATOM('m', 'o', 'o', 'v')
#define PNOT_ATOM QT_ATOM('p', 'n', 'o', 't')
#define SKIP_ATOM QT_ATOM('s', 'k', 'i', 'p')
#define WIDE_ATOM QT_ATOM('w', 'i', 'd', 'e')
#define PICT_ATOM QT_ATOM('P', 'I', 'C', 'T')
#define FTYP_ATOM QT_ATOM('f', 't', 'y', 'p')
#define UUID_ATOM QT_ATOM('u', 'u', 'i', 'd')

#define CMOV_ATOM QT_ATOM('c', 'm', 'o', 'v')
#define STCO_ATOM QT_ATOM('s', 't', 'c', 'o')
#define CO64_ATOM QT_ATOM('c', 'o', '6', '4')

#define ATOM_PREAMBLE_SIZE 8
#define COPY_BUFFER_SIZE 1024

int main(int argc, char *argv[])
{
    FILE *infile = 0;
    FILE *outfile = 0;
    unsigned char atom_bytes[ATOM_PREAMBLE_SIZE];
    uint32_t atom_type = 0;
    uint64_t atom_size = 0;
    uint64_t last_offset = 0;  // warning: 'last_offset' may be used uninitialized in this function
    unsigned char *moov_atom = 0;
    unsigned char *ftyp_atom = 0;
    uint64_t moov_atom_size = 0;  // warning: 'moov_atom_size' may be used uninitialized in this function
    uint64_t ftyp_atom_size = 0;
    uint64_t i, j;
    uint32_t offset_count;
    uint64_t current_offset;
    uint64_t start_offset = 0;
    unsigned char copy_buffer[COPY_BUFFER_SIZE];
    int bytes_to_copy;
    uint32_t last_atom_type = 0;
    uint64_t last_atom_size = 0;
    uint64_t last_atom_offset = 0;
    uint64_t atom_offset;
    int just_remove_free = 0;
    int return_code = 1;

    if (argc != 3) {
        printf ("Usage: qt_faststart <infile.mov> <outfile.mov>\n");
        goto exit_cleanup;
    }

    infile = fopen(argv[1], "rb");
    if (!infile) {
        perror(argv[1]);
        goto exit_cleanup;
    }

    /* traverse through the atoms in the file to make sure that 'moov' is
     * at the end */
    while (!feof(infile)) {
        if (fread(atom_bytes, ATOM_PREAMBLE_SIZE, 1, infile) != 1) {
            break;
        }
        atom_size = (uint32_t)BE_32(&atom_bytes[0]);
        atom_type = BE_32(&atom_bytes[4]);
        atom_offset = ftello(infile) - ATOM_PREAMBLE_SIZE;

        /* keep ftyp atom */
        if (atom_type == FTYP_ATOM) {
            ftyp_atom_size = atom_size;
            ftyp_atom = malloc(ftyp_atom_size);
            if (!ftyp_atom) {
                printf ("could not allocate 0x%"PRIX64" byte for ftyp atom\n",
                        atom_size);
                goto exit_cleanup;
            }
            fseeko(infile, -ATOM_PREAMBLE_SIZE, SEEK_CUR);
            if (fread(ftyp_atom, atom_size, 1, infile) != 1) {
                perror(argv[1]);
                goto exit_cleanup;
            }
            start_offset = ftello(infile);
        } else {
        /* 64-bit special case */
        if (atom_size == 1) {
            if (fread(atom_bytes, ATOM_PREAMBLE_SIZE, 1, infile) != 1) {
                break;
            }
            atom_size = BE_64(&atom_bytes[0]);
            fseeko(infile, atom_size - ATOM_PREAMBLE_SIZE * 2, SEEK_CUR);
        } else {
            fseeko(infile, atom_size - ATOM_PREAMBLE_SIZE, SEEK_CUR);
        }
        }

        printf("%c%c%c%c %10"PRIu64" %"PRIu64"\n",
               (atom_type >> 24) & 255,
               (atom_type >> 16) & 255,
               (atom_type >>  8) & 255,
               (atom_type >>  0) & 255,
               atom_offset,
               atom_size);

        if ((atom_type != FREE_ATOM) &&
            (atom_type != JUNK_ATOM) &&
            (atom_type != MDAT_ATOM) &&
            (atom_type != MOOV_ATOM) &&
            (atom_type != PNOT_ATOM) &&
            (atom_type != SKIP_ATOM) &&
            (atom_type != WIDE_ATOM) &&
            (atom_type != PICT_ATOM) &&
            (atom_type != UUID_ATOM) &&
            (atom_type != FTYP_ATOM)) {
            printf ("encountered non-QT top-level atom (is this a Quicktime file?)\n");
            break;
        }
        if ((atom_type != FREE_ATOM)) {
            last_atom_size = atom_size;
            last_atom_type = atom_type;
            last_atom_offset = ftello(infile) - last_atom_size;
        }
        if ((atom_type != FREE_ATOM) && (atom_type != MOOV_ATOM))
            last_offset = ftello(infile);
    }

    if (last_atom_type == MOOV_ATOM) {

        /* moov atom was, in fact, the last atom in the chunk; load the whole
         * moov atom */
        fseeko(infile, last_atom_offset, SEEK_SET);
        moov_atom_size = last_atom_size;
        moov_atom = malloc(moov_atom_size);
        if (!moov_atom) {
            printf ("could not allocate 0x%"PRIX64" byte for moov atom\n",
                atom_size);
            goto exit_cleanup;
        }
        if (fread(moov_atom, moov_atom_size, 1, infile) != 1) {
            perror(argv[1]);
            goto exit_cleanup;
        }

        /* this utility does not support compressed atoms yet, so disqualify
         * files with compressed QT atoms */
        if (BE_32(&moov_atom[12]) == CMOV_ATOM) {
            printf ("this utility does not support compressed moov atoms yet\n");
            goto exit_cleanup;
        }

        /* crawl through the moov chunk in search of stco or co64 atoms */
        for (i = 4; i < moov_atom_size - 4; i++) {
            atom_type = BE_32(&moov_atom[i]);
            if (atom_type == STCO_ATOM) {
                printf (" patching stco atom...\n");
                atom_size = BE_32(&moov_atom[i - 4]);
                if (i + atom_size - 4 > moov_atom_size) {
                    printf (" bad atom size\n");
                    free(moov_atom);
                    return 1;
                }
                offset_count = BE_32(&moov_atom[i + 8]);
                for (j = 0; j < offset_count; j++) {
                    current_offset = BE_32(&moov_atom[i + 12 + j * 4]);
                    current_offset += moov_atom_size;
                    moov_atom[i + 12 + j * 4 + 0] = (current_offset >> 24) & 0xFF;
                    moov_atom[i + 12 + j * 4 + 1] = (current_offset >> 16) & 0xFF;
                    moov_atom[i + 12 + j * 4 + 2] = (current_offset >>  8) & 0xFF;
                    moov_atom[i + 12 + j * 4 + 3] = (current_offset >>  0) & 0xFF;
                }
                i += atom_size - 4;
            } else if (atom_type == CO64_ATOM) {
                printf (" patching co64 atom...\n");
                atom_size = BE_32(&moov_atom[i - 4]);
                if (i + atom_size - 4 > moov_atom_size) {
                    printf (" bad atom size\n");
                    free(moov_atom);
                    return 1;
                }
                offset_count = BE_32(&moov_atom[i + 8]);
                for (j = 0; j < offset_count; j++) {
                    current_offset = BE_64(&moov_atom[i + 12 + j * 8]);
                    current_offset += moov_atom_size;
                    moov_atom[i + 12 + j * 8 + 0] = (current_offset >> 56) & 0xFF;
                    moov_atom[i + 12 + j * 8 + 1] = (current_offset >> 48) & 0xFF;
                    moov_atom[i + 12 + j * 8 + 2] = (current_offset >> 40) & 0xFF;
                    moov_atom[i + 12 + j * 8 + 3] = (current_offset >> 32) & 0xFF;
                    moov_atom[i + 12 + j * 8 + 4] = (current_offset >> 24) & 0xFF;
                    moov_atom[i + 12 + j * 8 + 5] = (current_offset >> 16) & 0xFF;
                    moov_atom[i + 12 + j * 8 + 6] = (current_offset >>  8) & 0xFF;
                    moov_atom[i + 12 + j * 8 + 7] = (current_offset >>  0) & 0xFF;
                }
                i += atom_size - 4;
            }
        }
    } else {
        if (atom_type == FREE_ATOM) {
            printf ("free atom at %"PRIu64" removed.\n", last_offset);
            just_remove_free = 1;
        } else {
            printf ("Last atom in file was not a moov or free atom.  Skipping.\n");
            goto exit_cleanup;
        }
    }

    outfile = fopen(argv[2], "wb");
    if (!outfile) {
        perror(argv[2]);
        goto exit_cleanup;
    }

    /* dump the same ftyp atom */
    if (ftyp_atom_size > 0) {
        printf (" writing ftyp atom...\n");
        if (fwrite(ftyp_atom, ftyp_atom_size, 1, outfile) != 1) {
            perror(argv[2]);
            goto exit_cleanup;
        }
    }

    if (!just_remove_free) {
        /* dump the new moov atom */
        printf (" writing moov atom...\n");
        if (fwrite(moov_atom, moov_atom_size, 1, outfile) != 1) {
            perror(argv[2]);
            goto exit_cleanup;
        }
    }

    /* copy the remainder of the infile, from offset 0 -> last_offset - 1 */
    printf (" copying rest of file: %"PRIu64"..%"PRIu64"\n", start_offset, last_offset);
    fseeko(infile, start_offset, SEEK_SET);
    last_offset -= start_offset;  /* Amount to copy */
    while (last_offset > 0) {
        if (last_offset > COPY_BUFFER_SIZE)
            bytes_to_copy = COPY_BUFFER_SIZE;
        else
            bytes_to_copy = last_offset;

        if (fread(copy_buffer, bytes_to_copy, 1, infile) != 1) {
            perror(argv[1]);
            goto exit_cleanup;
        }
        if (fwrite(copy_buffer, bytes_to_copy, 1, outfile) != 1) {
            perror(argv[2]);
            goto exit_cleanup;
        }
        last_offset -= bytes_to_copy;
    }
    return_code = 0;

exit_cleanup:
    if (infile)
        fclose(infile);
    if (outfile)
        fclose(outfile);
    if (moov_atom)
        free(moov_atom);
    if (ftyp_atom)
        free(ftyp_atom);
    return return_code;
}
