;//
;// 
;// File Name:  armSP_FFT_CToC_SC16_Radix4_ls_unsafe_s.s
;// OpenMAX DL: v1.0.2
;// Last Modified Revision:   7765
;// Last Modified Date:       Thu, 27 Sep 2007
;// 
;// (c) Copyright 2007-2008 ARM Limited. All Rights Reserved.
;// 
;// 
;//
;// Description:
;// Compute a Radix 4 FFT stage for a N point complex signal
;// 
;// 

        
;// Include standard headers

        INCLUDE omxtypes_s.h
        INCLUDE armCOMM_s.h
        INCLUDE armSP_FFT_s.h
        
        M_VARIANTS CortexA8
        
;// Import symbols required from other files
;// (For example tables)
    
        
        
        
;// Set debugging level        
;//DEBUG_ON    SETL {TRUE}


;// Guarding implementation by the processor name
    
    



    
;// Guarding implementation by the processor name
    
    IF  CortexA8
    
;// Import symbols required from other files
;// (For example tables)
    ;//IMPORT  armAAC_constTable    
    
;//Input Registers

pSrc            RN  0
pDst            RN  2
pTwiddle        RN  1
subFFTNum       RN  6
subFFTSize      RN  7



;//Output Registers


;//Local Scratch Registers

outPointStep     RN  3
grpCount         RN  4
dstStep          RN  5
pw1              RN  8
pw2              RN  9
pw3              RN  10   
pTmp             RN  4


;// Neon Registers

dButterfly1Real02   DN  D0.S16
dButterfly1Imag02   DN  D1.S16
dButterfly1Real13   DN  D2.S16
dButterfly1Imag13   DN  D3.S16
dButterfly2Real02   DN  D4.S16
dButterfly2Imag02   DN  D5.S16
dButterfly2Real13   DN  D6.S16
dButterfly2Imag13   DN  D7.S16
dXr0             DN  D0.S16
dXi0             DN  D1.S16
dXr1             DN  D2.S16
dXi1             DN  D3.S16
dXr2             DN  D4.S16
dXi2             DN  D5.S16
dXr3             DN  D6.S16
dXi3             DN  D7.S16

dW1rS32          DN  D8.S32         
dW1iS32             DN  D9.S32
dW2rS32             DN  D10.S32
dW2iS32             DN  D11.S32
dW3rS32             DN  D12.S32
dW3iS32             DN  D13.S32

dW1r             DN  D8.S16
dW1i             DN  D9.S16
dW2r             DN  D10.S16
dW2i             DN  D11.S16
dW3r             DN  D12.S16
dW3i             DN  D13.S16

dTmp0            DN  D12.S16
dTmp1             DN  D13.S16
dTmp1S32         DN  D13.S32
dTmp2S32         DN  D14.S32
dTmp3S32         DN  D15.S32

dYr0             DN  D18.S16
dYi0             DN  D19.S16
dYr1             DN  D16.S16
dYi1             DN  D17.S16
dYr2             DN  D20.S16
dYi2             DN  D21.S16
dYr3             DN  D14.S16
dYi3             DN  D15.S16
qY0              QN  Q9.S16
qY1              QN  Q8.S16
qY2              QN  Q10.S16
qY3              QN  Q7.S16

qX0              QN  Q0.S16
qX1              QN  Q1.S16
qX2              QN  Q2.S16
qX3              QN  Q3.S16

qT0              QN  Q9.S32
qT1              QN  Q10.S32
qT2              QN  Q7.S32
qT3              QN  Q8.S32

dZr0             DN  D22.S16
dZi0             DN  D23.S16
dZr1             DN  D24.S16
dZi1             DN  D25.S16
dZr2             DN  D26.S16
dZi2             DN  D27.S16
dZr3             DN  D28.S16
dZi3             DN  D29.S16

qZ0              QN  Q11.S16
qZ1              QN  Q12.S16
qZ2              QN  Q13.S16
qZ3              QN  Q14.S16

        
        MACRO
        FFTSTAGE $scaled, $inverse , $name
        
        ;// Define stack arguments
        
        MOV     pw2,pTwiddle 
        VLD4 {dW2r,dW2i,dTmp0,dTmp1},[pw2@256]!
        
        MOV     pw3,pTwiddle
        MOV     pw1,pTwiddle
        ;// pOut0+1 increments pOut0 by 8 bytes
        ;// pOut0+outPointStep == increment of 4*outPointStep bytes
        MOV     outPointStep,subFFTSize,LSL #2
        
        VLD3 {dW3rS32,dTmp1S32,dTmp2S32},[pw3@64]!
        MOV     subFFTNum,#1                            ;//after the last stage
        LSL     grpCount,subFFTSize,#2
                       
        
        ;// Update grpCount and grpSize rightaway 
        VLD3 {dW3iS32,dTmp2S32,dTmp3S32},[pw3@64]!
        
        ;// update subFFTSize for the next stage
        MOV     subFFTSize,grpCount
        MOV     dstStep,outPointStep,LSL #1
        
        VLD2 {dW1r,dW1i}, [pw1@128]!
              
        
        ADD     dstStep,dstStep,outPointStep                ;// dstStep = 3*outPointStep
        RSB     dstStep,dstStep,#16                         ;// dstStep = - 3*outPointStep+16
        
        VLD4     {dButterfly1Real02,dButterfly1Imag02,dButterfly1Real13,dButterfly1Imag13},[pSrc@256]! ;// AC.r AC.i BD.r BD.i
        VLD4     {dButterfly2Real02,dButterfly2Imag02,dButterfly2Real13,dButterfly2Imag13},[pSrc@256]! ;// AC.r AC.i BD.r BD.i
        
        ;// Process 4 groups at a time
        
grpLoop$name
        
                          
        ;// Rearrange the third twiddle
        VUZP    dW3r,dW3i
        SUBS    grpCount,grpCount,#16                    ;// grpCount is multiplied by 4
        
                
        VUZP     dButterfly1Real13, dButterfly2Real13        ;// B.r D.r
        VUZP     dButterfly1Imag13, dButterfly2Imag13        ;// B.i D.i
        VUZP     dButterfly1Real02, dButterfly2Real02        ;// A.r C.r
        VUZP     dButterfly1Imag02, dButterfly2Imag02        ;// A.i C.i
        
                
        IF  $inverse
            VMULL   qT0,dXr1,dW1r
            VMLAL   qT0,dXi1,dW1i                       ;// real part
            VMULL   qT1,dXi1,dW1r
            VMLSL   qT1,dXr1,dW1i                       ;// imag part
            
        ELSE
            VMULL   qT0,dXr1,dW1r
            VMLSL   qT0,dXi1,dW1i                       ;// real part
            VMULL   qT1,dXi1,dW1r
            VMLAL   qT1,dXr1,dW1i                       ;// imag part
        
        ENDIF
        
        ;// Load the first twiddle for 4 groups : w^1
        ;// w^1 twiddle (i+0,i+1,i+2,i+3)       for group 0,1,2,3
        
        VLD2 {dW1r,dW1i}, [pw1@128]!
        
        IF  $inverse
            VMULL   qT2,dXr2,dW2r
            VMLAL   qT2,dXi2,dW2i                       ;// real part
            VMULL   qT3,dXi2,dW2r
            VMLSL   qT3,dXr2,dW2i                       ;// imag part
            
        ELSE
            VMULL   qT2,dXr2,dW2r
            VMLSL   qT2,dXi2,dW2i                       ;// real part
            VMULL   qT3,dXi2,dW2r
            VMLAL   qT3,dXr2,dW2i                       ;// imag part
        
        ENDIF
        
        VRSHRN  dZr1,qT0,#15
        VRSHRN  dZi1,qT1,#15
        
        
        
        IF  $inverse
            VMULL   qT0,dXr3,dW3r
            VMLAL   qT0,dXi3,dW3i                       ;// real part
            VMULL   qT1,dXi3,dW3r
            VMLSL   qT1,dXr3,dW3i                       ;// imag part
            
        ELSE
            VMULL   qT0,dXr3,dW3r
            VMLSL   qT0,dXi3,dW3i                       ;// real part
            VMULL   qT1,dXi3,dW3r
            VMLAL   qT1,dXr3,dW3i                       ;// imag part
        
        ENDIF
        
        ;// Load the second twiddle for 4 groups : w^2
        ;// w^2 twiddle (2i+0,2i+2,2i+4,2i+6)   for group 0,1,2,3
        VLD4 {dW2r,dW2i,dTmp0,dTmp1},[pw2@256]!
        
        
        VRSHRN  dZr2,qT2,#15
        VRSHRN  dZi2,qT3,#15
        
        ;// Load the third twiddle for 4 groups : w^3
        ;// w^3 twiddle (3i+0,3i+3,3i+6,3i+9)   for group 0,1,2,3
        
        VLD3 {dW3rS32,dTmp1S32,dTmp2S32},[pw3@64]!
        
        VRSHRN  dZr3,qT0,#15
        VRSHRN  dZi3,qT1,#15
        
        VLD3 {dW3iS32,dTmp2S32,dTmp3S32},[pw3@64]!        
        
        IF $scaled
        
            ;// finish first stage of 4 point FFT 
            
            VHADD    qY0,qX0,qZ2
            VHSUB    qY2,qX0,qZ2
            VHADD    qY1,qZ1,qZ3
            VLD4    {dButterfly1Real02,dButterfly1Imag02,dButterfly1Real13,dButterfly1Imag13},[pSrc@256]! ;// AC.r AC.i BD.r BD.i
            
            VHSUB    qY3,qZ1,qZ3
                        
            ;// finish second stage of 4 point FFT 
            
            VHSUB    qZ0,qY2,qY1
            VHADD    qZ2,qY2,qY1
            VLD4     {dButterfly2Real02,dButterfly2Imag02,dButterfly2Real13,dButterfly2Imag13},[pSrc@256]! ;// AC.r AC.i BD.r BD.i
            
                                    
            IF $inverse
                
                VHADD    dZr3,dYr0,dYi3                          ;// y3 = u0-ju3
                VST2    {dZr0,dZi0},[pDst@128],outPointStep
                VHSUB    dZi3,dYi0,dYr3
                
                VHSUB    dZr1,dYr0,dYi3                          ;// y1 = u0+ju3
                VHADD    dZi1,dYi0,dYr3
                VST2    {dZr3,dZi3},[pDst@128],outPointStep
                VST2    {dZr2,dZi2},[pDst@128],outPointStep
                VST2    {dZr1,dZi1},[pDst@128],dstStep              ;// dstStep = -3*outPointStep + 16
            
            ELSE
                
                VHSUB    dZr1,dYr0,dYi3                          ;// y1 = u0+ju3
                VHADD    dZi1,dYi0,dYr3
            
                VHADD    dZr3,dYr0,dYi3                          ;// y3 = u0-ju3
                VST2    {dZr0,dZi0},[pDst@128],outPointStep
                VHSUB    dZi3,dYi0,dYr3
                VST2    {dZr1,dZi1},[pDst@128],outPointStep
                VST2    {dZr2,dZi2},[pDst@128],outPointStep
                VST2    {dZr3,dZi3},[pDst@128],dstStep              ;// dstStep = -3*outPointStep + 16
                
            ENDIF            
        
        ELSE
        
            ;// finish first stage of 4 point FFT 
            
            VADD    qY0,qX0,qZ2
            VSUB    qY2,qX0,qZ2
            VADD    qY1,qZ1,qZ3
            VLD4    {dButterfly1Real02,dButterfly1Imag02,dButterfly1Real13,dButterfly1Imag13},[pSrc@256]! ;// AC.r AC.i BD.r BD.i
            
            VSUB    qY3,qZ1,qZ3
                        
            ;// finish second stage of 4 point FFT 
            
            VSUB    qZ0,qY2,qY1
            VADD    qZ2,qY2,qY1
            VLD4     {dButterfly2Real02,dButterfly2Imag02,dButterfly2Real13,dButterfly2Imag13},[pSrc@256]! ;// AC.r AC.i BD.r BD.i
            
                                    
            IF $inverse
                
                VADD    dZr3,dYr0,dYi3                          ;// y3 = u0-ju3
                VST2    {dZr0,dZi0},[pDst@128],outPointStep
                VSUB    dZi3,dYi0,dYr3
                
                VSUB    dZr1,dYr0,dYi3                          ;// y1 = u0+ju3
                VADD    dZi1,dYi0,dYr3
                VST2    {dZr3,dZi3},[pDst@128],outPointStep
                VST2    {dZr2,dZi2},[pDst@128],outPointStep
                VST2    {dZr1,dZi1},[pDst@128],dstStep              ;// dstStep = -3*outPointStep + 16
            
            ELSE
                
                VSUB    dZr1,dYr0,dYi3                          ;// y1 = u0+ju3
                VADD    dZi1,dYi0,dYr3
            
                VADD    dZr3,dYr0,dYi3                          ;// y3 = u0-ju3
                VST2    {dZr0,dZi0},[pDst@128],outPointStep
                VSUB    dZi3,dYi0,dYr3
                VST2    {dZr1,dZi1},[pDst@128],outPointStep
                VST2    {dZr2,dZi2},[pDst@128],outPointStep
                VST2    {dZr3,dZi3},[pDst@128],dstStep              ;// dstStep = -3*outPointStep + 16
                
            ENDIF            
                        
            
            
                        
        ENDIF
        
        BGT     grpLoop$name
           

        ;// Reset and Swap pSrc and pDst for the next stage     
        MOV     pTmp,pDst
        SUB     pSrc,pSrc,#64                       ;// Extra increment currently done in the loop
        SUB     pDst,pSrc,outPointStep,LSL #2       ;// pDst -= size; pSrc -= 4*size bytes           
        SUB     pSrc,pTmp,outPointStep
                
        MEND
        
        
        M_START armSP_FFTFwd_CToC_SC16_Radix4_ls_OutOfPlace_unsafe,r4
        FFTSTAGE {FALSE},{FALSE},FWD
        M_END

        
        M_START armSP_FFTInv_CToC_SC16_Radix4_ls_OutOfPlace_unsafe,r4
        FFTSTAGE {FALSE},{TRUE},INV
        M_END
 
        
        M_START armSP_FFTFwd_CToC_SC16_Sfs_Radix4_ls_OutOfPlace_unsafe,r4
        FFTSTAGE {TRUE},{FALSE},FWDSFS
        M_END

        
        M_START armSP_FFTInv_CToC_SC16_Sfs_Radix4_ls_OutOfPlace_unsafe,r4
        FFTSTAGE {TRUE},{TRUE},INVSFS
        M_END

        
    ENDIF                                                           ;//CortexA8
    

    
     
    END