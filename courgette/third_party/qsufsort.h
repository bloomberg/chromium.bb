/*
  qsufsort.h -- Suffix array generation.

  Copyright 2003 Colin Percival

  For the terms under which this work may be distributed, please see
  the adjoining file "LICENSE".

  ChangeLog:
  2005-05-05 - Use the modified header struct from bspatch.h; use 32-bit
               values throughout.
                 --Benjamin Smedberg <benjamin@smedbergs.us>
  2010-05-26 - Use a paged array for V and I. The address space may be too
               fragmented for these big arrays to be contiguous.
                 --Stephen Adams <sra@chromium.org>
  2015-08-03 - Extract QSufSort to a separate file as template.
                 --Samuel Huang <huangs@chromium.org>
  2015-08-19 - Optimize split() and search(), add comments.
                 --Samuel Huang <huangs@chromium.org>
  2016-04-27 - Change split() to use Bentley & McIlroy's pivot selection
               algorithm, which QSufSort originally used. Reference:
               http://www.larsson.dogma.net/qsufsort.c
                 --Samuel Huang <huangs@chromium.org>
*/

#include <algorithm>
#include <cstring>

namespace courgette {
namespace qsuf {

// ------------------------------------------------------------------------
//
// The following code is taken verbatim from 'bsdiff.c'. Please keep all the
// code formatting and variable names.  The changes from the original are:
// (1) replacing tabs with spaces,
// (2) indentation,
// (3) using 'const',
// (4) changing the V and I parameters from int* to template <typename T>.
// (5) optimizing split() and search(); fix styles.
//
// The code appears to be a rewritten version of the suffix array algorithm
// presented in "Faster Suffix Sorting" by N. Jesper Larsson and Kunihiko
// Sadakane, special cased for bytes.

namespace {

template <typename T> T median3(const T& a, const T& b, const T& c) {
  if (a < b)
    return b < c ? b : (a < c ? c : a);
  return b > c ? b : (a > c ? c : a);
}

}  // namespace

template <typename T> void split(T I, T V, int start, int end, int h) {
  // For small interval, apply selection sort.
  if (end - start < 16) {
    for (int i = start; i < end; ) {
      int skip = 1;
      int best = V[I[i] + h];
      for (int j = i + 1; j < end; j++) {
        int cur = V[I[j] + h];
        if (best > cur) {
          best = cur;
          int tmp = I[i];
          I[i] = I[j];
          I[j] = tmp;
          skip = 1;
        } else if (best == cur) {
          int tmp = I[i + skip];
          I[i + skip] = I[j];
          I[j] = tmp;
          ++skip;
        }
      }
      if (skip == 1) {
        V[I[i]] = i;
        I[i] = -1;
      } else {
        for (int j = i, jend = i + skip; j < jend; j++)
          V[I[j]] = jend - 1;
      }
      i += skip;
    }
    return;
  }

  // Select pivot, algorithm by Bentley & McIlroy.
  int n = end - start;
  int mid = start + (n >> 1);
  int pivot = V[I[mid] + h];
  int p1 = V[I[start] + h];
  int p2 = V[I[end - 1] + h];
  if (n > 40) {  // Big array: Pseudomedian of 9.
    int s = n >> 3;
    pivot = median3(pivot, V[I[mid - s] + h], V[I[mid + s] + h]);
    p1 = median3(p1, V[I[start + s] + h], V[I[start + s + s] + h]);
    p2 = median3(p2, V[I[end - 1 - s] + h], V[I[end - 1 - s - s] + h]);
  }  // Else medium array: Pseudomedian of 3.
  pivot = median3(pivot, p1, p2);

  // Split [start, end) into 3 intervals:
  //   [start, j) with secondary keys < pivot,
  //   [j, k) with secondary keys == pivot,
  //   [k, end) with secondary keys > pivot.
  int j = start;
  int k = end;
  for (int i = start; i < k; ) {
    int cur = V[I[i] + h];
    if (cur < pivot) {
      if (i != j) {
        int tmp = I[i];
        I[i] = I[j];
        I[j] = tmp;
      }
      ++i;
      ++j;
    } else if (cur > pivot) {
      --k;
      int tmp = I[i];
      I[i] = I[k];
      I[k] = tmp;
    } else {
      ++i;
    }
  }

  // Recurse on the "< pivot" piece.
  if (start < j)
    split<T>(I, V, start, j, h);

  // Update the "== pivot" piece.
  if (j == k - 1) {
    V[I[j]] = j;
    I[j] = -1;
  } else {
    for (int i = j; i < k; ++i)
      V[I[i]] = k - 1;
  }

  // Recurse on the "> pivot" piece.
  if (k < end)
    split<T>(I, V, k, end, h);
}

template <class T>
static void
qsufsort(T I, T V,const unsigned char *old,int oldsize)
{
  int buckets[256];
  int i,h,len;

  for(i=0;i<256;i++) buckets[i]=0;
  for(i=0;i<oldsize;i++) buckets[old[i]]++;
  for(i=1;i<256;i++) buckets[i]+=buckets[i-1];
  for(i=255;i>0;i--) buckets[i]=buckets[i-1];
  buckets[0]=0;

  for(i=0;i<oldsize;i++) I[++buckets[old[i]]]=i;
  I[0]=oldsize;
  for(i=0;i<oldsize;i++) V[i]=buckets[old[i]];
  V[oldsize]=0;
  for(i=1;i<256;i++) if(buckets[i]==buckets[i-1]+1) I[buckets[i]]=-1;
  I[0]=-1;

  for(h=1;I[0]!=-(oldsize+1);h+=h) {
    len=0;
    for(i=0;i<oldsize+1;) {
      if(I[i]<0) {
        len-=I[i];
        i-=I[i];
      } else {
        if(len) I[i-len]=-len;
        len=V[I[i]]+1-i;
        split<T>(I,V,i,i+len,h);
        i+=len;
        len=0;
      };
    };
    if(len) I[i-len]=-len;
  };

  for(i=0;i<oldsize+1;i++) I[V[i]]=i;
}

static int
matchlen(const unsigned char *old,int oldsize,const unsigned char *newbuf,int newsize)
{
  int i;

  for(i=0;(i<oldsize)&&(i<newsize);i++)
    if(old[i]!=newbuf[i]) break;

  return i;
}

template <class T>
static int search(T I, const unsigned char *old, int oldsize,
                  const unsigned char *newbuf, int newsize, int *pos) {
  int lo = 0;
  int hi = oldsize;
  while (hi - lo >= 2) {
    int mid = (lo + hi) / 2;
    if(memcmp(old+I[mid],newbuf,std::min(oldsize-I[mid],newsize))<0) {
      lo = mid;
    } else {
      hi = mid;
    }
  }

  int x = matchlen(old + I[lo], oldsize - I[lo], newbuf, newsize);
  int y = matchlen(old + I[hi], oldsize - I[hi], newbuf, newsize);
  if(x > y) {
    *pos = I[lo];
    return x;
  }
  *pos = I[hi];
  return y;
}

//  End of 'verbatim' code.
// ------------------------------------------------------------------------

}  // namespace qsuf
}  // namespace courgette
